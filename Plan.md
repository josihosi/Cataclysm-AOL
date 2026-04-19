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

There is now **one active greenlit implementation lane**: **Combat-oriented locker policy**.

Fresh checkpoint just closed:
- **Package 5, basecamp carried-item dump lane** is honestly closed on the documented `bandages` acceptance item.
- **Package 4, locker zone policy + control-surface cleanup** is now honestly reclosed too:
  - `.userdata/dev-harness/harness_runs/20260419_141422/` shows the real McWilliams `CAMP_LOCKER` Zone Manager seam creating `Basecamp: Locker`, renaming it to `Probe Locker`, closing through the single-`Esc` save prompt, and reopening Zone Manager with `Probe Locker` still present
  - that same live packet did **not** surface the reported zone-creation type-mismatch on screen, in `debug.final.log`, or in stderr
- **Organic bulletin-board speech polish** is now honestly reclosed too:
  - deterministic coverage still passes for the cleaned spoken board/job bark and widened organic status parsing
  - live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed Katharina answering `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
  - that same live packet still had Robbie chime in as ordinary follower crosstalk on the McWilliams fixture, but no fresh machine-speech seam appeared

Current target:
- move the now-active combat-oriented locker policy lane through its next honest narrow slice after the explicit ballistic-maintenance footing and first direct lighter-upper-body displacement slices landed
- the current tree now has explicit combat-policy footholds instead of vague roadmap prose:
  - `camp_locker_slot`, `all_camp_locker_slots()`, locker policy persistence, and `locker_policy_ui()` already expose explicit `gloves`, `mask`, `belt`, and `holster` controls
  - the locker policy surface now also has a persisted `Prefer bulletproof gear` toggle that raises body-armor and helmet bullet-resistance weighting instead of leaving ballistic preference as hidden scoring folklore
  - body-armor scoring now also treats loaded ablative plates as real ballistic value, and same-type ballistic vests can upgrade over damaged-plate variants instead of pretending equal item ids mean equal armor state
  - protective full-body suits now cover the current lighter-upper-body seam: they suppress missing-current shirt filler when chosen from the locker, and they can also displace weaker currently worn shirts and vests when the suit itself is the meaningful pants-lane upgrade
  - the current locker footing still keeps the useful safety scaffolding, like weird-garment preservation, weather sensitivity, and full-body suit protection, so the next slice should extend that spine instead of replacing it
- keep this separate from the already-closed board speech cleanup, deeper locker V3 doctrine soup, and bandit/threat design lanes

Best concrete next states for this lane:
1. extend the direct current-outfit seam beyond shirts and vests, most likely into body-armor tradeoffs only when the full-body suit is clearly superior overall
2. keep that next code proof deterministic first, centered on explicit combat-suit-vs-current-body-armor tradeoff behavior, before demanding live locker service proof
3. only broaden into wider doctrine/fashion nuance once the fuller combat-suit preference packet is real on the current tree

Explicit greenlit backlog behind the current slice:
- **Plan/Aux pipeline helper**
- **Bandit concept formalization follow-through** (bottom-of-stack, conceptual docs only)

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

## 5. Checkpointed — Locker zone policy + control-surface cleanup

**Status:** CHECKPOINTED / DONE FOR NOW

This slice is now considered done for now because the bundled Locker Zone V1 surface/control row in `SUCCESS.md` is checked.

Current honest state:
- the suspicion-first audit and fresh deterministic packet already reclosed the non-live claims:
  - `CAMP_LOCKER` remains the real `Basecamp: Locker` Zone Manager zone definition
  - locker policy state still exists and the player-facing `Camp_Locker_Policy` / `basecamp::locker_policy_ui()` surface still exists
  - ordinary sorting still leaves `CAMP_LOCKER` tiles alone
- fresh live proof now exists at `.userdata/dev-harness/harness_runs/20260419_141422/`:
  - the real McWilliams Zone Manager path creates `Basecamp: Locker`
  - the creation-time rename to `Probe Locker` works on screen
  - single-`Esc` closeout opens the save prompt, `Y` returns to gameplay, and reopening Zone Manager still shows `Probe Locker`
  - no type-mismatch popup or related stderr/debug failure surfaced on that packet

Keep this slice closed unless later code or runtime evidence breaks one of those claims.

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

## 9. Checkpointed, organic bulletin-board speech

**Status:** CHECKPOINTED / DONE FOR NOW

This slice is now considered done for now because the bundled success state in `SUCCESS.md` is checked.

Current honest state:
- ordinary player-facing board status asks now accept organic wording like `what needs making`, `what needs doing`, `got any craft work`, and `show me what needs doing`
- ordinary spoken board/job replies stay free of visible request-id glue and keep `show_board` / `show_job` / `job=` structure on the internal/log side only
- deterministic coverage still passes on the current tree for the widened organic parsing and cleaned spoken bark
- live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed Katharina answering `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
- the same live packet still had Robbie chime in as ordinary follower crosstalk on the McWilliams fixture; treat that as separate routing noise unless a later change makes it the active seam again

Keep this lane closed unless a fresh probe surfaces a real visible machine-speech seam again.

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

## 11. Active — combat-oriented locker policy

**Status:** ACTIVE / GREENLIT

Josef wants locker development to lean harder toward sensible guard/combat outfits without throwing away the already-earned weird-garment safety wins.

The current narrow combat-policy slices are now landed on the current tree:
- `camp_locker_slot`, `all_camp_locker_slots()`, and the persisted locker-policy surface now include explicit `gloves`, `mask`, `belt`, and `holster` slots instead of the older 12-slot-only footing
- `classify_camp_locker_item()` now gives common combat support gear an explicit locker-policy home instead of dropping holsters on the floor or letting belt-like waist gear get lost in generic clothing logic
- the locker policy surface now also has a persisted `Prefer bulletproof gear` toggle, and body-armor / helmet scoring now leans harder toward higher bullet protection when that control is enabled
- body-armor scoring now also counts loaded ablative plates as real armor value, and same-type ballistic vests can be upgraded over damaged-plate variants instead of treating identical item ids as an automatic tie
- focused deterministic coverage now exercises loaded-vs-empty ballistic vest scoring, damaged insert scoring, and same-type healthy-plate replacement behavior on the current tree
- protective full-body suits in the pants lane now also suppress missing-current shirt filler when the suit itself is already the better combat/protective packet, so survivor-style suits stop inviting junk T-shirts underneath them just because the shirt lane is empty
- that same protective-suit seam now also handles the current lighter-upper-body comparisons: when the suit wins the pants-lane upgrade honestly, weaker current shirts and vests can be demoted into duplicate cleanup instead of being layered under the new suit
- the current locker safety spine stayed intact while doing that: weird-garment preservation, weather-sensitive outerwear/legwear handling, full-body suit protection, and the earlier great-helm / holster safety cases still survive the filtered locker suite
- broader explicit full-body battle/protective suit preference is still missing, so the active seam is no longer the ballistic-maintenance rule itself but the remaining suit-vs-current-body-armor slice built on top of it

What this active lane should do next:
- prefer clearly superior full-body battle suits over the remaining inferior currently worn body-armor cases when appropriate instead of stopping at shirts and vests
- keep extending deterministic combat/guard outfit proof before asking for live locker service proof
- bias future deterministic tests toward combat/guard outfit behavior rather than endlessly widening exotic garment law

Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.

---

## 12. Greenlit bottom-of-stack — bandit concept formalization follow-through

**Status:** GREENLIT / BOTTOM-OF-STACK

Josef wants Andi to start tightening the remaining bandit control-law gaps after the current basecamp fixes are honestly closed, but still as conceptualization/doc work rather than implementation.

This follow-through should turn the still-loose bandit economy, cadence, and cross-layer interaction rules into explicit packets without reopening code or pretending the whole bandit system is now active.

Frozen footing for this follow-through:
- no passive decay for bounty
- no passive decay for threat
- structural bounty is set by terrain / tile class and reduced by harvesting or exploitation
- moving bounty changes when real actors or visible activity change, not because an idle timer shaved numbers off in the dark
- bandit camp stockpile may go down by consumption, and any future spoilage rule must stay explicit and separate from bounty/threat logic

Default package order:
1. bounty source / harvesting / camp-stockpile rules
2. cadence / distance burden / route-fallback rules
3. cross-layer interaction packet with starter numbers and worked scenarios

The auxiliary contract intentionally decomposes those three packages into **31 narrow single-question micro-items** so Andi can freeze one law at a time instead of hiding several assumptions inside one friendly paragraph.

Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.

Do not treat this as permission to start coding bandit AI.
This is greenlit **concept formalization** at the bottom of the stack, meant to make the parked packet cleaner and implementation-legible later.

---

## 13. Parked concept chain — bandit overmap AI

**Status:** PARKED / CONCEPT CHAIN

Josef wants the larger bandit / overmap-threat idea developed as a parked concept chain first, then re-evaluated for greenlight only after the concept packet is coherent enough as a whole.
Do not quietly treat partial bandit notes as an active lane or as already-greenlit implementation.
The new bottom-of-stack formalization follow-through above is greenlit only for doc/spec cleanup inside this chain; implementation still stays parked.

Current parked-chain anchor:
- the broad synthesis paper lives at `doc/bandit-overmap-ai-concept-2026-04-19.md`

Current parked sub-items:
- deterministic bounty/threat scoring guidance v1 at `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- overmap mark-generation and heatmap model v1 at `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- bidirectional overmap-to-bubble handoff seam v1 at `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- player/basecamp visibility and concealment v1 at `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`

Supporting recon note for the visibility item:
- physical-systems recon at `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

What the current parked sub-items should do:
- scoring guidance: define deterministic camp-ledger and map-mark inputs for bandit decision-making, then score bounty/threat/job desirability from those inputs
- mark/heatmap guidance: define how overmap-only marks get created, rechecked, harvested, and folded into broad threat/bounty heatmaps on the same cadence family without passive decay math
- handoff guidance: define how abstract overmap groups enter local play and how cargo, wounds, panic, losses, and updated threat/bounty knowledge return back into overmap state
- visibility/concealment guidance: define signal sources, environmental filters, bounty/threat interpretation outputs, and player/basecamp exposure-reduction levers on recon-backed footing
- current acceptable footing: fog/mist can suppress sight/light legibility without forcing new fog-based sound dampening rules
- resolved note-1 anti-spiral footing: each camp should own its own sparse operational map rather than shared omniscience
- resolved note-1 bounty footing: ground bounty should be coarse, structural, finite, and non-regenerating by region class, while player/NPC/camp activity supplies the moving bounty layer
- resolved no-passive-decay footing: do not model passive decay for bounty or threat; structural bounty changes through harvesting/exploitation, moving bounty changes through fresh activity and collection, threat changes through real recheck, and camp stockpile changes through explicit consumption
- resolved note-1 dampers: false-lead / recently-checked / harvested memory and self-signal filtering should stop camps from spiraling into pseudo-psychic fixation
- resolved note-2 fairness footing: sustained pressure and stalking against legible camps is desired, but decisive full camp assault stays player-present only for current scope
- resolved note-2 attack-signaling footing: fairness should come from bounded offscreen consequence scope, not from requiring bandits to presignal attacks
- resolved note-2 repo footing: existing overmap-NPC persistence, travel, and companion plumbing is valid substrate for following outbound groups and route interception, but the current need-driven wandering policy is not the finished hostile model
- resolved note-3 identity footing: persistent overmap group continuity is mandatory, with only a small anchored-individual slice surviving handoffs directly while the remaining membership stays fungible
- resolved note-3 goal-sustainability footing: smoke/mark destinations are provisional mission leads, not sacred tile commitments, and local observations may continue, divert, shadow, or abort the current goal
- resolved note-3 scope footing: v1 should cap anchored continuity at roughly 1-3 individuals rather than forcing full per-bandit persistence everywhere
- resolved note-4 city-opportunism footing: cities may support one-off or occasional opportunistic bandit action under zombie chaos, but not persistent farming, because structural bounty depletes while threat remains high
- resolved note-4 zombie-model footing: zombies currently matter as threat and target-coherence pressure only, not as a separately simulated bandit-versus-zombie tactical system
- resolved note-4 sticky-threat footing: if a group leaves due to threat, the area should keep a sticky scary mark that is only meaningfully reevaluated on close revisit or when some other attraction pulls the group back nearby
- resolved note-5 independence footing: v1 camps should behave as mostly independent actors rather than a coordinated coalition strategy layer, so overlap may happen by accident but routine dogpile convergence should not be the norm
- resolved note-5 anti-dogpile footing: territoriality, distance burden, depletion, sticky threat, and fresh active-pressure penalties should suppress repeated multi-camp pile-ons against the same region
- resolved note-5 scope footing: deliberate alliances, campaign planning, and explicit multi-camp coordination are later-layer strategy material, not part of the current parked v1 shape

The intended parked-chain order for now is:
1. broad concept vessel
2. deterministic bounty/threat scoring
3. overmap mark-generation and heatmap model
4. bidirectional overmap-to-bubble handoff seam
5. player/basecamp visibility and concealment, informed by the physical-systems recon note
6. only then decide whether the whole bandit concept is ready to be promoted into greenlit backlog

The broad anchor doc has now been rewritten into the synthesis paper for the parked chain.
If the packet is revisited later, the next planning discussion should be about whether the packet is coherent enough for future greenlight consideration, not about spawning more disconnected feeder docs by default.

---

## 14. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
