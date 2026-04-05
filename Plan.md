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

## 1. Current delivery target — LLM-side board snapshot path

**Status:** ACTIVE

### Goal
Wire the richer board handoff snapshot into the real structured / LLM-side path without polluting the short spoken camp bark.
The intended product split is:
- live natural speech `show me the board` stays concise and human-facing
- the richer `planner_move` + overmap snapshot belongs in the structured / LLM-side handoff path

### What is already true
- The snapshot builder and tests already know how to include `planner_move` and an overmap snapshot when an origin is available.
- The real prompt template on disk now also includes `{{planning_snapshot}}` again, so the shipped prompt matches the code-side snapshot contract.
- `./tests/cata_test "[camp][basecamp_ai]"` passed (`269 assertions in 1 test case`) after the prompt-template sync.

### What is still missing
The missing proof is not another spoken-board wording pass.
What still needs to be shown is that the richer board snapshot enters the **actual structured / LLM-side path** with the right evidence, while the human-facing spoken board reply stays concise.

### Exit criteria for this target
Before this target can move out of active status:
1. prove where the richer board handoff snapshot is emitted in the real structured / LLM-side path
2. show that the prompt path carries the planning snapshot / overmap context when appropriate
3. keep the short spoken board bark on the human-facing path
4. document the current truth in `TODO.md` and `TESTING.md` without bloating them

### Immediate next move
Trace the current `show_board` / handoff path end-to-end and capture the first honest evidence packet for the LLM-side snapshot lane.
Prefer the smallest proof that answers the routing question — likely targeted deterministic evidence or artifact/log confirmation — instead of another broad live ritual.

---

## 3. Locker Zone v1

**Status:** CHECKPOINTED

Locker Zone v1 is no longer the active lane.
It now has the pieces that were missing:
- live current-binary downtime proof on `dev` / `Sandy Creek`
- reviewable checkpoints instead of one giant dirty locker blob
- current ledger/testing notes aligned to that state

Keep it out of the active queue unless later code changes break the locker path again.

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
