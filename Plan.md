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

## 1. Current delivery target — no new post-Locker-V1 lane greenlit yet

**Status:** WAITING FOR NEXT PICK

Locker Zone **v1** was the full greenlit lane.
By the repo's current V1 definition, that lane is now checkpointed / done for this pass:
- zone + policy + menu/save-load plumbing landed
- classifier + candidate gathering + scoring landed
- queue/reservation + one-at-a-time servicing landed
- live locker execution + duplicate cleanup + obvious upgrades landed
- dirty-trigger follow-through landed
- deterministic coverage and proportional runtime validation are in place

Important clarification:
- `dirty-trigger follow-through` was the **last V1 chunk**, not the whole greenlit lane by itself
- do **not** rewrite history so V1 sounds like a tiny sub-slice that still needed separate blessing

Current rule:
- do **not** reopen finished Locker Zone v1 or the finished Basecamp bark / craft / board work unless Josef explicitly wants another slice there
- the next real move should come from the parked menu below instead of inventing a surprise lane

### Immediate next move
- if Josef greenlights another agent-side lane, take it from the parked options below
- otherwise keep the ledgers honest and report that Locker Zone v1 is checkpointed / done-for-now instead of pretending there is still an active queue

---

## 2. Checkpointed — LLM-side board snapshot path

**Status:** CHECKPOINTED

This slice reached its exit criteria for now:
- routing proof exists on the actual camp request router, not only on helper builders
- the richer structured/internal `show_board` lane is covered with deterministic evidence
- the short spoken board bark stayed separate
- the testing/docs packet can now describe current truth instead of an open routing question

Keep this out of the active queue unless later code changes break the route again or a new greenlit slice explicitly extends it.

---

## 3. Parked options available for greenlight

If Josef is not available and a new agent-side lane is wanted, offer a short menu from here instead of inventing a new roadmap.

1. **Board/job artifact proof cleanup**
   - tighten the explicit `DEBUG_LLM_INTENT_LOG` board/job artifact path so the live debug packet is as legible as the deterministic router proof
2. **Upstream deterministic Basecamp cleanup**
   - prune and package the deterministic board/job handoff work for cleaner upstream review/reference material
3. **Broader LLM-side board prompt follow-through**
   - extend the same legible structured-path treatment beyond `show_board` only if Josef explicitly wants the next slice

Keep the menu short and concrete: what it is, why it is parked, and what one greenlit next chunk would be.

---

## 4. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
