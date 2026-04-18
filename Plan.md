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

## Current status

There is now **one active greenlit implementation lane**: **Locker Package 5, basecamp carried-item dump lane**.

Current target:
- make basecamp NPCs dump ordinary carried junk before or during locker dressing instead of preserving follower-style pocket clutter
- keep only a deliberately tiny carried lane for now: `bandages`, `ammo`, and `magazines`
- keep the dump behavior out of curated locker stock and on the current `McWilliams` / `Zoraida Vick` footing
- do **not** silently blend this with Package 4 surface-control cleanup, bandit/overmap-threat design, or hackathon feature lanes

Best concrete next states for this lane:
1. keep the landed Package 5 code/test packet honest in the docs: ordinary carried junk now dumps on the locker service path, and the deterministic keep-vs-dump policy is intentionally limited to `bandages`, `ammo`, and `magazines`
2. package the remaining live proof on the right locker footing so the real McWilliams / `CAMP_LOCKER` path explicitly shows the kept carried lane too, not only the junk-dump side
3. if the current fixture does not naturally contain the kept classes, use the narrowest honest harness restage/helper to seed only the exact proof items on that same live path; this is explicitly greenlit as part of Package 5, not Package 4 drift
4. close the slice only after that live packet exists, without pretending Package 4 was secretly finished too

Explicit greenlit backlog behind the current slice:
- **Package 4, locker zone policy + control-surface cleanup**
- **Organic bulletin-board speech polish**
- **Combat-oriented locker policy**
- **Plan/Aux pipeline helper**

Meaning:
- these items are defined enough and explicitly greenlit
- they do **not** need another permission round
- they are **not** all active at once
- Andi should still be judged against one chosen current slice, not allowed to freestyle across the buffet

---

## 1. Checkpointed — Smart Zone Manager v1

**Status:** CHECKPOINTED / DONE FOR NOW

Smart Zone Manager v1 is now good enough for the current pre-freeze goal.
The lane should stay closed unless later code/runtime evidence disproves the claimed state.

Current honest state:
- the one-off Smart Zone Manager v1 surface is real on both Basecamp inventory-zone creation and later position/stretch edits
- the deterministic packet still proves the main stamped layout, the corrected firewood/splintered/wood knot, too-small-zone failure, outdoor rotten placement beyond a simple wall ring, non-destructive refusal on pre-zoned bed tiles, and repeatability on the same layout
- the contract seam audit is now closed honestly: fire/food/equipment anchors are flag-first where the map exposes real signals, clothing/bed support still use small explicit id allowlists where the map does not expose a clean category signal, and ordinary floor fallback stays in place instead of clever failure
- existing built-in loot/custom zone machinery is still the default shape; the only current deliberate custom filters in the v1 packet are `splintered`, `dirty`, `rotten`, `blanket`, and `quilt`
- a proportional live McWilliams proof now exists on the rebuilt current tiles binary via `tools/openclaw_harness/scenarios/smart_zone.live_probe_mcw_prepped.json`, using a narrow prepared-save restage (clear existing zones, spawn/deploy a brazier, place `Basecamp: Storage`, accept the smart-zoning prompt, save, reopen Zone Manager)
- the live packet stayed narrow: it proved the real prompt path plus the saved stamped layout without reopening zone-doctrine archaeology

Keep this lane out of the active queue unless later work breaks one of those claims.

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

## 5. Active — Basecamp carried-item dump lane

**Status:** ACTIVE / GREENLIT

The current active slice is **Package 5, basecamp carried-item dump lane** from `doc/locker-basecamp-followthrough-work-packages-2026-04-07.md`.

Still believed true unless new evidence breaks it:
- locker outfitting core exists as real planner/service behavior
- locker maintenance rhythm exists as real dirty/queue/reservation behavior
- locker ranged-readiness support already covers ammo / magazine use from curated locker stock
- earlier deterministic + proportional runtime proof for those non-carried-item slices still exists

Current required close-out for this slice:
- basecamp NPCs dump ordinary carried misc junk before or during the locker dressing cycle
- the kept carried lane is intentionally tiny and explicit:
  - bandages
  - ammo
  - magazines
- dump behavior does **not** pollute curated locker stock
- the result behaves as a basecamp-specific policy instead of follower-style inventory preservation

Current honest state:
- the locker service code now strips ordinary carried junk during locker dressing instead of preserving generic follower-style pocket clutter
- the kept carried lane is explicitly narrow in code and deterministic coverage: `bandages`, `ammo`, and `magazines`
- replaced/duplicate container contents now get split so kept carried items return to the worker while dumped miscellany goes to a non-locker cleanup tile
- fresh runtime-compatible `locker.weather_wait` artifact proof on 2026-04-18 shows the live locker path really dumping ordinary carried junk outside curated locker stock
- the remaining gap is that the current live locker fixture proved the junk-dump side but did **not** explicitly exercise the kept `bandages` / `ammo` / `magazines` lane yet, so Package 5 is not honestly closed

Greenlit backlog, not erased:
- Package 4 locker zone policy + control-surface cleanup remains a known unfinished slice and is now explicitly greenlit, just not the current active queue

Out of scope for this slice:
- finishing Package 4 as part of the same patch just because it is nearby
- grenades or broader consumable logic
- complex pocket micromanagement
- bandit / overmap-threat design
- hackathon feature lanes

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

## 8. Hackathon feature lanes — parked far back / do not touch anytime soon

These lanes are **not part of the current basecamp work and not part of the near-term queue**.
Keep them visibly separate so scaffolding/support work is not mistaken for partial feature delivery.

1. **Chat interface over in-game dialogue branches**
   - future feature lane, parked far back
2. **Tiny ambient-trigger NPC model**
   - future feature lane, parked far back

Do not reopen them during the current locker/basecamp slice.
Do not treat them as the next natural follow-up after Package 5.
Current ordering intent is that they stay buried until much later threat/worldwork exists, with Josef explicitly saying they are not for anytime soon and not before the later Mongol riders lane.
Do not describe adjacent harness or UI work as partial completion of those features.

---

## 9. Greenlit backlog — organic bulletin-board speech

**Status:** GREENLIT / BACKLOG

The raw structured board/job payload leak is fixed, but one aesthetic remainder is still visible on the bulletin-board path: the surviving `job=1`-style follow-up wording still sounds like internal routing glue instead of organic in-world speech.

This lane is greenlit for later because the path is already stable enough to leave alone for now, but the remaining player-facing machine-speech ugliness is a real defined item.

What this future polish item should do:
- support organic player-facing triggers for bulletin-board / camp-job requests (for example ordinary requests like "craft" in a sentence)
- keep internal routing/debug tokens available where useful without surfacing them as normal speech
- make visible answers sound like poor survivors in a dump making it work for another day while the dead and worse roam outside
- eliminate remaining visible `job=<id>` / `show_board` / `show_job`-style machine phrasing from ordinary in-world output

Canonical contract lives at `doc/organic-bulletin-board-speech-2026-04-09.md`.

---

## 10. Greenlit backlog — Plan/Aux pipeline helper

**Status:** GREENLIT / BACKLOG TOOLING

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

## 11. Greenlit backlog — combat-oriented locker policy

**Status:** GREENLIT / BACKLOG

Josef wants future locker development to lean harder toward sensible guard/combat outfits instead of spending disproportionate energy on weird artisanal clothing edge cases.

This future direction should:
- keep the already-earned weird-garment safety wins
- add common useful gear emphasis (gloves, belts, masks, holsters, sensible normal outfit pieces)
- support a bulletin-board bulletproof toggle
- support explicit ballistic vest / plate handling
- prefer clearly superior full-body battle suits when appropriate
- bias future deterministic tests toward combat/guard outfit behavior rather than endlessly widening exotic garment law

Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.

---

## 12. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
