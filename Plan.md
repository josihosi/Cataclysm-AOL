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

## 1. Current delivery target — post-Locker-V1 Basecamp follow-through

**Status:** GREENLIT / ACTIVE

Josef has now greenlit the previously parked three-part Basecamp follow-through queue too.
So stop pretending the repo is waiting for a pick.

Current active queue, in order:
1. **Upstream deterministic Basecamp cleanup**
   - prune/package the existing deterministic board/job work into a cleaner upstream-ready shape
2. **Broader LLM-side board prompt follow-through**
   - extend the richer structured treatment beyond `show_board`

Recently landed:
- **Board/job artifact proof cleanup**
  - the live `DEBUG_LLM_INTENT_LOG` board/job packet now fences the exact handoff text with `reply_begin` / `reply_end`, so the live artifact is easier to scan and compare against the deterministic router proof

Working rules:
- these three are already greenlit; do **not** move them back into a permission menu
- work them in order unless reality forces a better order and the docs are updated to explain why
- keep finished Locker Zone v1 and the finished Basecamp bark / craft / board checkpoint closed unless Josef explicitly reopens them

### Immediate next move
- move to **Upstream deterministic Basecamp cleanup**
- keep the broader prompt follow-through as the queued next slice, not as a vague future wish
- keep `Plan.md`, `SUCCESS.md`, `TODO.md`, and `TESTING.md` aligned to the now-two-step active queue plus the landed artifact cleanup proof

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

## 3. Additional parked options

None right now.

Do not create a fake menu while the active three-part Basecamp follow-through queue is still open.
If that queue finishes and Josef wants more, add a fresh short menu then.

---

## 4. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
