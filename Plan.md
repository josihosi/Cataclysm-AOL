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

## 1. Current delivery target — Smart Zone Manager v1

**Status:** ACTIVE / CURRENT LANE

Josef explicitly greenlit Smart Zone Manager v1 as the next active lane.
The locker / basecamp follow-through packet is good enough for now and should not keep expanding just because more clothing weirdness exists.

Current truth:
- the current job is now **Smart Zone Manager v1**, using `doc/smart-zone-manager-v1-aux-plan-2026-04-06.md` as the canonical product contract
- the one-off Smart Zone Manager v1 surface is now wired on Basecamp inventory-zone placement plus later position/stretch edits, so the lane is no longer blocked on the basic action existing at all
- the current deterministic packet now proves the main stamped layout, the corrected firewood/splintered/wood knot, too-small-zone failure, outdoor rotten placement beyond simple wall rings, non-destructive refusal on pre-zoned bed tiles, and repeatability on the same layout
- this lane should still optimize for **works, does not break the game, and saves player clicks**, not for endless edge-case archaeology or giant map-planner ambition
- anti-testing-madness rule still applies: the remaining work is to close the honest contract gaps and get the live packet, not to turn Smart Zone Manager into another proof cult

Current required shape:
- preserve the already-agreed zone layout contract from the Smart Zone Manager v1 auxiliary doc
- keep anchor choice flag/category-first where the map exposes real signals, with floor fallback instead of clever failure
- keep placement deterministic and non-destructive by default
- do not widen into patrol/locker/general camp automation
- McWilliams currently has no ready fire-source anchor, so the harness/live proof may need a narrow restaging step to place/activate a brazier or equivalent fire-source anchor before the crafting niche can be proved honestly

### Next real state
1. close the remaining contract seams around anchor-selection/reuse wording instead of pretending the lane is already fully reviewer-clean
2. prove the main discussed zone layout honestly on a live McWilliams-style setup, including a real fire-source anchor path for the crafting niche
3. keep the evidence packet narrow: current deterministic proof plus one proportional live packet, not a new garden of edge cases
4. once Smart Zone Manager v1 is stable enough for the pre-freeze goal, stop and return to freeze discipline instead of farming extra zoning nuance

---

## 2. Checkpointed — Patrol Zone v1

**Status:** CHECKPOINTED / DONE FOR NOW

This lane is now considered done for now because the bundled success state in `SUCCESS.md` is checked:
- patrol zone surface + planner + sticky-shift contract exist
- deterministic hold-vs-loop runtime behavior exists
- current-binary live proof exists for disconnected-loop and connected-hold cases
- the packaged patrol packet is now legible enough to explain gaps / off-shift state without leaning on raw trace logs alone

If later code work or runtime evidence shows any one of those claims is false or incomplete, reopen Patrol Zone v1 immediately.

---

## 3. Checkpointed — Locker-capable harness restaging

**Status:** CHECKPOINTED / DONE FOR NOW

This lane is now considered done for now because the bundled success state in `SUCCESS.md` is checked:
- a real locker-capable fixture/restaging path exists
- `locker.weather_wait` is no longer blocked on missing fixture shape
- a fresh packaged run reports **screen** / **tests** / **artifacts** separately
- the result is documented reviewer-cleanly as harness/fixture work on existing locker behavior

If later fixture drift, harness drift, or locker runtime evidence breaks any one of those bundled claims, reopen this lane immediately.

---

## 4. Checkpointed — Locker Zone V2

**Status:** CHECKPOINTED / DONE FOR NOW

V2 is now considered done for now because the bundled V2 task set in `SUCCESS.md` is checked:
- managed ranged loadouts can pull up to two compatible magazines from locker supply
- selected compatible magazines can be topped off from locker-zone ammo and the supported weapon reloaded from that supply
- deterministic coverage exists for the V2 contract
- proportional runtime proof is recorded on the current binary

If later code work or runtime evidence shows any one of those bundled claims is false or incomplete, reopen V2 immediately.

---

## 5. Reopened context — Locker Zone V1

**Status:** REOPENED UNDER SECTION 1

Do not treat Locker Zone V1 as closed right now.
Fresh-save manual testing disproved the old surface/control close-out, so the active V1 reopen now lives in section 1.

Still believed true unless new evidence breaks it:
- locker outfitting core exists as real planner/service behavior
- locker maintenance rhythm exists as real dirty/queue/reservation behavior
- earlier deterministic + proportional runtime proof for those non-surface slices still exists

What is no longer safe to claim as closed until revalidated:
- that the locker surface/control is currently solid on the real fresh-save path
- that ordinary sorting cannot siphon gear out of locker tiles
- that the current locker zone interaction surface is free of the reported type-mismatch / overlay problems

---

## 6. Checkpointed — post-Locker-V1 Basecamp follow-through

**Status:** CHECKPOINTED / DONE FOR NOW

This queue reached its exit criteria for now:
- the board/job log packet is legible enough to compare against the deterministic router proof
- the deterministic board packaging is cleaner/upstream-friendlier
- the richer structured treatment now follows the board-emitted `next=` tokens instead of dropping straight back to spoken bark
- the testing/docs packet describes the closed state instead of an open queue

Keep this closed unless Josef explicitly reopens Basecamp prompt follow-through or a later change breaks the structured board/job lane again.

---

## 7. Checkpointed — LLM-side board snapshot path

**Status:** CHECKPOINTED

This slice reached its exit criteria for now:
- routing proof exists on the actual camp request router, not only on helper builders
- the richer structured/internal `show_board` lane is covered with deterministic evidence
- the short spoken board bark stayed separate
- the testing/docs packet can now describe current truth instead of an open routing question

Keep this out of the active queue unless later code changes break the route again or a new greenlit slice explicitly extends it.

---

## 8. Hackathon-reserved feature lanes — do not touch before the event

These are intentionally **reserved for the hackathon itself**.
They should stay visibly separate from the current repo-footing/harness work so reviewers do not mistake scaffolding for early feature implementation.

1. **Chat interface over in-game dialogue branches**
   - the future feature lane
   - current harness work may exercise nearby-NPC/freeform chat controls, but that is only test scaffolding and **not** this feature
2. **Tiny ambient-trigger NPC model**
   - the future feature lane
   - current harness work may stage weird-item scenarios and artifact checks, but that is only test scaffolding/observability and **not** this feature

Do not start them early, do not half-land them, and do not describe scaffolding as partial completion.

---

## 9. Parked future polish — organic bulletin-board speech

**Status:** PARKED / FUTURE POLISH

The raw structured board/job payload leak is fixed, but one aesthetic remainder is still visible on the bulletin-board path: the surviving `job=1`-style follow-up wording still sounds like internal routing glue instead of organic in-world speech.

This lane is parked for later because the current freeze-closeout packet only needs the path to be stable and non-catastrophic, not fully beautiful.

What this future polish item should do:
- support organic player-facing triggers for bulletin-board / camp-job requests (for example ordinary requests like "craft" in a sentence)
- keep internal routing/debug tokens available where useful without surfacing them as normal speech
- make visible answers sound like poor survivors in a dump making it work for another day while the dead and worse roam outside
- eliminate remaining visible `job=<id>` / `show_board` / `show_job`-style machine phrasing from ordinary in-world output

Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.

---

## 10. Parked future tooling — Plan/Aux pipeline helper

**Status:** PARKED / FUTURE TOOLING

Josef explicitly wants a small helper for the `Plan.md` / auxiliary-doc pipeline because greenlighting already-existing lanes should not require slow manual file carpentry every time.

What this future tool should do:
- take a proposed item, greenlight, or parked-lane request
- print the contract back for verification
- collect corrections before touching canon files
- ask the final classification question (active, parked, or bottom-of-stack)
- patch the relevant canon files consistently:
  - `Plan.md`
  - `TODO.md`
  - `SUCCESS.md`
  - `TESTING.md` when needed
  - the auxiliary doc itself
- optionally generate the Andi handoff packet too

The point is leverage and consistency, not ceremony:
- preserve the frozen intake/classification/packaging workflow
- reduce slow manual canon edits
- make parked-vs-active-vs-bottom-of-stack updates faster and less error-prone

Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.

---

## 11. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
