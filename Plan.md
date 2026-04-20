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

There is now **one active greenlit lane**: **Bandit concept formalization follow-through**.

Fresh checkpoints just closed:
- **Plan/Aux pipeline helper** is honestly checkpointed:
  - `tools/plan_aux_pipeline_helper.py` now lets `emit` produce reviewer-visible aux/canon snippets plus optional downstream `andi.handoff.md` output from the same validated classified contract
  - the bounded patch path still stays on known existing headings instead of pretending broad freeform canon mutation is solved
  - narrow validation passed on a sample spec via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted handoff review, and `apply` on a temp repo copy
- **Combat-oriented locker policy** is honestly checkpointed:
  - the explicit `gloves`, `mask`, `belt`, and `holster` locker slots are real, the `Prefer bulletproof gear` toggle is real, ballistic vest scoring/replacement now notices loaded vs damaged ablative plates, and the full-body combat-suit packet now reaches weaker current shirts, vests, and body armor while stronger ballistic armor still stays put
  - the final suspicion-first audit found one real remaining proof gap, namely that the new common combat-support slots had classification/candidate coverage but no direct service proof yet
  - that gap is now closed by deterministic test `camp_locker_service_equips_common_combat_support_slots`, which drives real `CAMP_LOCKER` service and shows gloves, dust mask, tool belt, and holster actually being equipped from locker stock
  - focused deterministic validation passed on the current tree: `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`
- **Package 5, basecamp carried-item dump lane** is honestly closed on the documented `bandages` acceptance item.
- **Package 4, locker zone policy + control-surface cleanup** is now honestly reclosed too:
  - `.userdata/dev-harness/harness_runs/20260419_141422/` shows the real McWilliams `CAMP_LOCKER` Zone Manager seam creating `Basecamp: Locker`, renaming it to `Probe Locker`, closing through the single-`Esc` save prompt, and reopening Zone Manager with `Probe Locker` still present
  - that same live packet did **not** surface the reported zone-creation type-mismatch on screen, in `debug.final.log`, or in stderr
- **Organic bulletin-board speech polish** is now honestly reclosed too:
  - deterministic coverage still passes for the cleaned spoken board/job bark and widened organic status parsing
  - live run `.userdata/dev-harness/harness_runs/20260419_154244/` used the real camp-assignment seam, asked `what needs making`, and showed Katharina answering `Board's got 1 live and 1 old - 1 x bandages.` with no visible request-id glue
  - that same live packet still had Robbie chime in as ordinary follower crosstalk on the McWilliams fixture, but no fresh machine-speech seam appeared

Current target:
- continue the active bandit formalization follow-through as doc/spec work only
- Package 1, micro-item 1, `Terrain bounty bucket table`, is now landed in the follow-through packet with explicit 0-5 starter buckets for open street, field, forest, cabin, house, farm, city structure, and camp footprint
- Package 1, micro-item 2, `Structural bounty harvest trigger rule`, is now landed too: mere visitation/scouting does not deplete structural bounty; only actual extraction, stripping, or meaningful denial of site-bound value does
- Package 1, micro-item 3, `Structural bounty reappearance rule`, is now landed too: structural bounty does not passively grow back and only returns when the site itself gains new site-bound value through real resupply, rebuilding, or reoccupation
- Package 1, micro-item 4, `Moving bounty source table`, is now landed too: moving bounty now explicitly names direct human sightings, repeated route traffic, caravans/haul convoys, basecamp routine, smoke, ordinary visible light, defensive scanning light, and sound/contact clues as separate live source families
- Package 1, micro-item 5, `Moving bounty attachment rule`, is now landed too: moving bounty now stays source-shaped instead of smearing into terrain, with explicit actor/group, route/corridor, and site-centered activity attachments
- Package 1, micro-item 6, `Moving bounty clear / rewrite rule`, is now landed too: moving bounty cleanup now happens by evidence-driven refresh, overwrite, clear, replacement, or coexistence on the source carrier rather than by fake passive melt
- Package 1, micro-item 7, `Threat source table`, is now landed too: threat marks now explicitly come from direct humans, organized defenders, fortification signs, searchlights/watch routines, combat-contact cues, failed probes, recent losses, and zombie/monster pressure instead of from generic bounty clues
- Package 1, micro-item 8, `Threat rewrite rule`, is now landed too: threat now rewrites source-by-source through real recheck only, with explicit raise / confirm / lower cases instead of passive threat melt
- Package 1, micro-item 9, `Threat-and-bounty coexistence rule`, is now landed too: bounty and threat now stay as separate concurrent reads, so one region/carrier can remain worth attacking and risky to approach at the same time unless a source-specific recheck actually disproves one side
- Package 1, micro-item 10, `Destination-only vs along-route collection rule`, is now landed too: outings may skim opportunistic value on the current corridor or immediate intercept envelope, but not turn into vacuum-cleaner side-sweep harvesting or free goal rewrites
- Package 1, micro-item 11, `Collection yield rule`, is now landed too: one outing converts at most 0-3 haul steps into stockpile, route-side skims stay inside that same cap, and rich regions still need repeat exploitation instead of one magic vacuum pass
- Package 1, micro-item 12, `Camp stockpile consumption rule`, is now landed too: camp stockpile now drains only by explicit daily housekeeping plus explicit dispatch/return mission costs, with pressure coming from uncovered named costs rather than hidden decay
- Package 1, micro-item 13, `Forest yield rule`, is now landed too: pure forest now cashes out as thin food/fuel/basic-material value, usually 0-1 haul, while richer hidden sites in the woods should score as those site classes rather than as magical forest bounty
- Package 2, micro-item 14, `Daily movement budget rule`, is now landed too: daily travel is now frozen as a job-shaped 1-6 OMT/day envelope from local forage skim through rare strategic redeploy, instead of vague "a few tiles" mush or player-distance-ring canon drift
- Package 2, micro-item 15, `Cadence budget-spend rule`, is now landed too: cadence now spends only elapsed-time-earned travel credit from that daily budget, carrying fractional progress forward instead of rounding each wake into fake free OMT movement
- Package 2, micro-item 16, `Distance burden rule`, is now landed too: target desirability now falls by the round-trip share of the outing's daily travel budget, so the same destination reads cheaper for a raid than for a scout and far mediocre targets stop tying with nearby ones by accident
- Package 2, micro-item 17, `Return-clock rule`, is now landed too: each outing now carries a job-shaped elapsed-time leash, fresh sightings do not refresh it for free, and once the remaining clock shrinks to plain return time the default should flip toward home/disengage
- keep the next pass narrow: Package 2, micro-item 18, `Cargo / wounds / panic burden rule`
- answer only which burden factors shorten outings or reduce useful movement, and only touch adjacent stale wording when that burden pass directly forces the correction
- do not drift into the later Package 2 fallback/diversion law, code, or reopened locker/basecamp slices while doing this

Explicit greenlit backlog behind the current slice:
- **Plan status summary command**
  - read-only plan visibility tooling for `/plan active`, `/plan greenlit`, and `/plan parked`, with optional combined `/plan`
  - keep numbering meaningful, especially for the greenlit order, and fold bottom-of-stack items into that order instead of surfacing a separate command

Meaning:
- the bandit follow-through lane is now the only active slice
- the only greenlit backlog item behind it right now is the plan status summary command
- it does **not** need another permission round
- it is doc/spec work only, not code greenlight
- Andi should still move one micro-item at a time instead of freestyling across the buffet

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

## 10. Checkpointed — Plan/Aux pipeline helper

**Status:** CHECKPOINTED / DONE FOR NOW

Josef explicitly wanted a small helper for the `Plan.md` / auxiliary-doc pipeline so already-understood lanes stop requiring slow manual canon carpentry every time.

Current honest state:
- `tools/plan_aux_pipeline_helper.py` now provides a spec-driven preview/merge/emit/apply path for this repo's Plan/Aux workflow
- `emit` can now also write a terse downstream `andi.handoff.md` packet from the same validated classified contract when `handoff_needed` is `yes`
- the `apply` path still stays bounded to known existing canon headings and current active-lane anchors, so the workflow law is real without pretending broad freeform mutation is solved
- when a heading does not already exist, the honest fallback is still `emit` output for reviewer-visible paste/review instead of magical guessing
- narrow validation passed on a sample spec via `python3 -m py_compile tools/plan_aux_pipeline_helper.py`, `schema`, `show`, `emit`, emitted handoff review, and `apply` on a temp repo copy

Keep this lane closed unless later evidence shows the handoff output or bounded patch path lying about what the canon actually says.

Canonical contract lives at `doc/plan-aux-pipeline-helper-2026-04-09.md`.

---

## 11. Checkpointed — combat-oriented locker policy

**Status:** CHECKPOINTED / DONE FOR NOW

Josef wants locker development to lean harder toward sensible guard/combat outfits without throwing away the already-earned weird-garment safety wins.

The current narrow combat-policy slices are now landed on the current tree:
- `camp_locker_slot`, `all_camp_locker_slots()`, and the persisted locker-policy surface now include explicit `gloves`, `mask`, `belt`, and `holster` slots instead of the older 12-slot-only footing
- `classify_camp_locker_item()` now gives common combat support gear an explicit locker-policy home instead of dropping holsters on the floor or letting belt-like waist gear get lost in generic clothing logic
- the locker policy surface now also has a persisted `Prefer bulletproof gear` toggle, and body-armor / helmet scoring now leans harder toward higher bullet protection when that control is enabled
- body-armor scoring now also counts loaded ablative plates as real armor value, and same-type ballistic vests can be upgraded over damaged-plate variants instead of treating identical item ids as an automatic tie
- focused deterministic coverage now exercises loaded-vs-empty ballistic vest scoring, damaged insert scoring, same-type healthy-plate replacement behavior, and the positive/negative full-body-suit-vs-current-body-armor tradeoffs on the current tree
- protective full-body suits in the pants lane now also suppress missing-current shirt filler when the suit itself is already the better combat/protective packet, so survivor-style suits stop inviting junk T-shirts underneath them just because the shirt lane is empty
- that same protective-suit seam now also handles the direct current-outfit comparisons all the way through weaker body armor: when the suit wins the pants-lane upgrade honestly, weaker current shirts, vests, and body armor can be demoted into duplicate cleanup instead of being layered under the new suit, while stronger current ballistic armor remains equipped
- the current locker safety spine stayed intact while doing that: weird-garment preservation, weather-sensitive outerwear/legwear handling, full-body suit protection, and the earlier great-helm / holster safety cases still survive the filtered locker suite
- the final closure audit found one real remaining proof gap, namely end-to-end service evidence for the newly explicit combat-support slots
- that gap is now closed by deterministic test `camp_locker_service_equips_common_combat_support_slots`, which proves real `CAMP_LOCKER` service equips gloves, dust mask, tool belt, and holster from locker stock
- focused deterministic validation passed on the current tree via `make -j4 tests`, `./tests/cata_test "camp_locker_service_equips_common_combat_support_slots"`, and `./tests/cata_test "[camp][locker]"`

Keep this lane closed unless later code or runtime evidence breaks one of those claims.

Canonical contract lives at `doc/locker-combat-oriented-policy-2026-04-09.md`.

---

## 12. Active — Bandit concept formalization follow-through

**Status:** ACTIVE / GREENLIT DOC/SPEC

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

Current bounded slice:
- landed: Package 1, micro-item 1, `Terrain bounty bucket table`, now freezes a 0-5 starter site-class prior with the intended ordering `open street/field < forest < cabin < house < farm/city structure < camp footprint`
- landed: Package 1, micro-item 2, `Structural bounty harvest trigger rule`, now freezes that structural bounty only depletes once bandits actually extract, strip, or meaningfully deny site-bound value; scouting, visitation, and failed approaches do not count
- landed: Package 1, micro-item 3, `Structural bounty reappearance rule`, now freezes that depleted structural bounty stays down until the world adds new site-bound value back through real resupply, rebuilding, or reoccupation; fresh traffic or signals only restore moving interest
- landed: Package 1, micro-item 4, `Moving bounty source table`, now freezes the starter live source families for moving bounty instead of leaving humans, traffic, caravans, basecamp routine, smoke, light, and sound cues as one blurry blob
- landed: Package 1, micro-item 5, `Moving bounty attachment rule`, now freezes that moving bounty stays source-shaped, with actor/group, route/corridor, and site-centered activity attachments instead of being dumped back into structural terrain value
- landed: Package 1, micro-item 6, `Moving bounty clear / rewrite rule`, now freezes that moving bounty is cleaned up by evidence-driven refresh, overwrite, clear, replacement, or coexistence on the current carrier rather than by passive timer shave
- landed: Package 1, micro-item 7, `Threat source table`, now freezes that threat comes from direct humans, organized defenders, fortification signs, searchlights/watch routines, combat-contact cues, failed probes, recent losses, and zombie/monster pressure rather than from ordinary bounty-first activity clues
- landed: Package 1, micro-item 8, `Threat rewrite rule`, now freezes that threat only changes through source-specific recheck, with explicit raise / confirm / lower cases and no passive decay shortcut
- landed: Package 1, micro-item 9, `Threat-and-bounty coexistence rule`, now freezes that threat and bounty remain separate concurrent reads on the same region/carrier when the evidence supports both, with only source-specific contradiction clearing one side
- landed: Package 1, micro-item 10, `Destination-only vs along-route collection rule`, now freezes that outings may collect opportunistically on the current corridor or immediate intercept envelope, but not by broad side-sweep harvesting or free mission rewrites
- landed: Package 1, micro-item 11, `Collection yield rule`, now freezes a 0-3 per-outing haul cap so ingress stays bounded, route-side skims stay inside the same packet, and rich regions still need repeat exploitation
- landed: Package 1, micro-item 12, `Camp stockpile consumption rule`, now freezes one daily housekeeping drain plus explicit dispatch/return mission costs, with food/ammo/med pressure driven by those named costs rather than by hidden stockpile decay
- landed: Package 1, micro-item 13, `Forest yield rule`, now freezes forest as thin food/fuel/basic-material value, usually 0-1 haul, unless the outing actually touches a richer embedded site that should score by its own site class
- landed: Package 2, micro-item 14, `Daily movement budget rule`, now freezes a job-shaped 1-6 OMT/day starter envelope so local forage skims, probes, convoy hits, scavenge runs, raids, and rare redeploys stop sharing one mushy "few tiles" assumption
- landed: Package 2, micro-item 15, `Cadence budget-spend rule`, now freezes travel as elapsed-time-earned credit so cadence wakes can spend only what real world time has accrued instead of minting fresh movement every pass
- landed: Package 2, micro-item 16, `Distance burden rule`, now freezes target desirability as a round-trip-share discount against the outing's daily travel budget instead of a flat map-distance tax
- landed: Package 2, micro-item 17, `Return-clock rule`, now freezes a calm-condition outing leash with starter job clocks and a plain-return threshold, so groups prefer home before one chase turns into endless free pressure
- next: Package 2, micro-item 18, `Cargo / wounds / panic burden rule`
- keep the pass to one burden rule before touching the later Package 2 fallback/diversion law

Canonical contract lives at `doc/bandit-concept-formalization-followthrough-2026-04-19.md`.

Do not treat this as permission to start coding bandit AI.
This is now the active **concept formalization** lane, still doc/spec work only, meant to make the parked packet cleaner and implementation-legible later.

---

## 13. Greenlit backlog — Plan status summary command

**Status:** GREENLIT / BACKLOG TOOLING

Josef wants a small read-only command surface that prints current plan categories from `Plan.md` so active, greenlit, and parked work stop feeling like a black box when Andi or Schani reshuffle canon.

What this item should do:
- provide a compact plan-status readout with `active`, `greenlit`, and `parked` views, plus an optional combined `/plan`
- treat `Plan.md` as the only source of truth rather than chat history, agent summaries, or Andi's self-reporting
- keep numbered output where ordering matters, especially for the greenlit stack, with bottom-of-stack items folded in by position rather than split into a separate command
- warn when canon headings or status labels are too contradictory or too thin to classify cleanly instead of inventing certainty

Non-goals:
- do not mutate canon or reclassify items from the command path
- do not infer plan state from chat memory when `Plan.md` disagrees
- do not create a separate bottom-of-stack command or broaden this into workflow redesign

Canonical contract lives at `doc/plan-status-summary-command-2026-04-20.md`.

---

## 14. Parked concept chain — Bandit overmap AI

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

## 15. Documentation discipline

If the structure starts bloating again, apply this rule:
- `Plan.md` should be readable in a minute
- `TODO.md` should show only the current execution queue
- `TESTING.md` should show only current policy, latest relevant evidence, and pending probes
- `COMMIT_POLICY.md` should stop the dirty tree from becoming a lifestyle

If a sentence exists only to remember that something used to be true, it probably belongs in git history, not here.
