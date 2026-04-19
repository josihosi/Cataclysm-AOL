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
2. keep the corrected live packet honest on the real McWilliams tile: Robbie-only `say` targeting is now real, the shorter spoken order really submits on-screen, the bag-only probe proved `small plastic bag` is visibly collectible on its own, and the repaired `look_around` cap now lets the four-item seed complete together
3. keep the downstream repair honest too: the real post-pickup continuation again reaches `TALK_CAMP`, opens the worker-priority overlay, assigns Robbie back to camp, and returns to actual locker service on the same corrected packet
4. use the rebuilt locker readout to close the remaining exact `bandages` acceptance seam: the live artifact now names carried outcomes, so the next honest step is restaging the real `bandage` item instead of the auto-selected `adhesive bandage`, or explicitly changing policy if adhesive bandages are meant to count

Explicit greenlit backlog behind the current slice:
- **Package 4, locker zone policy + control-surface cleanup**
- **Organic bulletin-board speech polish**
- **Combat-oriented locker policy**
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
- the earlier mixed-actor worry is still not honestly closed on the corrected live packet; the current blocker has moved upstream from pure post-pickup dialog mapping to pickup targeting itself
- the probe helper itself was also lying for a while: `tools/openclaw_harness/startup_harness.py` had been leaving the multidrop menu open, and the drop path was selecting filtered items wrong, so the old post-drop screenshots were not honest ground checks
- fresh repaired single-count bandage proof is now real:
  - `.userdata/dev-harness/harness_runs/20260419_035025/` shows the wish helper stocks inventory for query `bandage`, as `adhesive bandage`
  - `.userdata/dev-harness/harness_runs/20260419_040122/` now shows the corrected filtered drop path really lands `adhesive bandage` on the live McWilliams pickup tile
- fresh repaired single-count magazine proof is now real too:
  - `.userdata/dev-harness/harness_runs/20260419_035431/` shows inventory really contains `Glock 9x19mm 15-round magazine (15/15 9x19mm JHP)` after the wish spawn
  - `.userdata/dev-harness/harness_runs/20260419_040243/` now shows the corrected filtered drop path really lands that Glock magazine on the live pickup tile
- the older `9x19mm JHP` wish-query mismatch is now demoted to historical context, not the current blocker:
  - `.userdata/dev-harness/harness_runs/20260419_031430/` showed the wish-menu query preferring `1000 9x19mm JHP ammo can`
  - `.userdata/dev-harness/harness_runs/20260419_040812/` showed the second filtered result still landing `small ammo can`
  - the current correction packet no longer relies on that exact query; it seeds `9x19mm JHP, reloaded` instead
- fresh same-actor ground proof at `.userdata/dev-harness/harness_runs/20260419_051921/` now shows the exact intended small-packet seed on the live pickup tile: `9x19mm JHP, reloaded (1)`, `adhesive bandage`, `Glock 9x19mm 15-round magazine (15/15 9x19mm JHP)`, and `small plastic bag`
- the overlong exact-name pickup sentence was a real harness/probe lie: `.userdata/dev-harness/harness_runs/20260419_051921/` stays stuck in `Enter a sentence to say`, while the shorter wording already used in the side probes and now in the corrected e2e scenario does submit on-screen
- fresh rerun at `.userdata/dev-harness/harness_runs/20260419_052751/` moved the correction packet one step forward:
  - the shorter spoken order is visibly submitted on-screen
  - Robbie visibly accepts the pickup instruction and starts acting on it
  - the generic follow-up packet still ended in the wrong place, but the artifact log at least proved the talk path was now reaching `TALK_MISSION_INQUIRE`, `TALK_MISSION_SUCCESS_LIE`, and `TALK_FRIEND` instead of staying trapped in the say prompt
- fresh dialog drill-down at `.userdata/dev-harness/harness_runs/20260419_055401/` did prove something useful, but less final than it first looked:
  - on that narrow probe, the post-pickup reply chain `b -> a -> b -> d -> n -> a` reached Robbie's camp-specific `What about the camp?` submenu and then `I want to assign you to work at this camp.`
  - so the camp branch is real on the corrected packet, but that probe alone was not enough to claim the packaged e2e path was now solved
- fresh packaged rerun at `.userdata/dev-harness/harness_runs/20260419_061324/` disproved the simpler overlay-exit story:
  - the same corrected seed packet still gets the shorter spoken order on-screen and the follow-up packet does reach Robbie dialogue again
  - but the inlined assignment continuation does not reproducibly reach the camp submenu on the full packet
  - in this rerun, the follow-up keys drift back into Robbie's general follower dialog: `d` selects `I'd like to know a bit more about your abilities.`, `a` opens Robbie's character sheet, and `c` opens `Select a destination`
  - no `camp locker:` artifact lines follow, so there is still no honest kept-lane readout from the corrected packet
- fresh re-audit at `.userdata/dev-harness/harness_runs/20260419_063750/` did catch a real upstream problem:
  - the short spoken order still submitted on-screen, but the message log showed both nearby followers responding
  - Katharina said `Got it, Zoraida Robbie, bandage and ammo go to you; I'll grab the Glock mag and keep watch.`
  - Robbie said `On it, Zoraida-I'll secure the supplies and keep watch while you grab the bandage and ammo.`
  - so that older `say` packet was not honestly Robbie-only
- the whisper fallback at `.userdata/dev-harness/harness_runs/20260419_064258/` also failed as a rescue path:
  - `You whisper "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."` is visibly submitted on-screen
  - after 50 turns, no follower response or pickup-completion line appears in the message log, so that targeted whisper path is a no-op on this footing
- fresh code repair plus rerun at `.userdata/dev-harness/harness_runs/20260419_073418/` moved the active seam forward again:
  - `src/npctalk.cpp` now filters allied hearers by direct addressed name before ambient routing steals the utterance, so named `say` orders stop fanning out to nearby followers
  - the corrected `say` packet is now honestly Robbie-only on-screen: `You say "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."`, Robbie is the only follower who answers, and only Robbie pickup lines appear in the visible log
  - that rerun also exposed the remaining upstream lie cleanly: the `look_around` prompt text and `parse_look_around_response()` were both still hard-capped at three selected items, so the fourth requested pickup never entered the queue
- Frau Knackal's bag-specific recommendation was then followed on the bounded targeting checkpoint at commit `377c1f0695`:
  - exactly one materially changed bag-only probe at `.userdata/dev-harness/harness_runs/20260419_082431/` restaged just `small plastic bag` on the same real McWilliams footing and ordered only `Robbie, pick up the small plastic bag on the ground.`
  - that bag-only probe answers the narrow question honestly: the bag is **not** being picked up silently. Robbie visibly picks it up on-screen, `config/llm_intent.log` records `look_around selected Robbie Knox (req_1): item_6:a: small plastic bag` and `look_around pickup Robbie Knox (small plastic bag): picked up 1 item(s)`, and the follow-up `g` check says `There is nothing to pick up nearby.`
- fresh four-item-cap repair validation at `.userdata/dev-harness/harness_runs/20260419_090351/` closes the upstream pickup-completion seam enough to move downstream again:
  - screen: `You say "Robbie, pick up the bandage, the reloaded 9mm ammo, the Glock magazine, and the small plastic bag on the ground."` is visibly submitted, Robbie is the only follower who answers, and the visible log shows Robbie picking up `adhesive bandage`, `9x19mm JHP, reloaded`, `Glock 9x19mm 15-round magazine`, and `small plastic bag`
  - artifacts/logs: `config/llm_intent.log` now records `look_around response Robbie Knox (req_1): item_8, item_6, item_7, item_9` and the corresponding four-item selection list; pickup lines are logged for `9x19mm JHP, reloaded`, `Glock 9x19mm 15-round magazine`, and `small plastic bag`
  - remaining weirdness is now observability, not queueing: the bandage still reaches the selected queue and is visibly picked up on-screen, but the current `action_status` logging falls through to `pickup.item_missing` instead of mirroring that pickup cleanly in `config/llm_intent.log`
- fresh downstream dialog re-probe at `.userdata/dev-harness/harness_runs/20260419_104954/` reclosed the next seam honestly:
  - after mission success, Robbie really does pass through `TALK_FRIEND_GUARD` first (`I'm on watch.`), but the correct continuation on the repaired footing is still `b -> d -> n -> a`
  - that probe again reaches `TALK_CAMP` and opens the worker-priority overlay, so the earlier drift probes turned out to be wrong-menu archaeology rather than a broken camp branch
- fresh full same-actor rerun at `.userdata/dev-harness/harness_runs/20260419_105232/` then carried the corrected packet all the way back into locker service:
  - screen: Robbie completes the four-item pickup, gets assigned back to `Bugchaser central`, returns to gameplay, and later visibly dons `Fighters' Glock 19` during service
  - artifacts/logs: `probe.artifacts.log` captured the real `camp locker:` packet again, proving the packet had returned to the correct downstream seam even though the old runtime still compressed the carried-item readout
- fresh observability patch plus rebuilt live rerun at `.userdata/dev-harness/harness_runs/20260419_111134/` closed that locker-readout seam for real:
  - `src/basecamp.cpp` now logs exact dumped carried items and exact kept carried items in the `camp locker:` cleanup summary; the narrow guard stayed green on the rebuilt tree (`make -j4 tests`, `./tests/cata_test "[camp][locker]"`, `make -j4 TILES=1 cataclysm-tiles`)
  - on the rebuilt live binary `6fa48c5e6c-dirty`, the same corrected packet now shows the exact outcome: `9x19mm JHP, reloaded` and `Glock 9x19mm 15-round magazine` stay with Robbie, while `small plastic bag` and `adhesive bandage` dump to cleanup
- fresh exact-bandage query probe at `.userdata/dev-harness/harness_runs/20260419_111843/` exposed the next remaining seam cleanly:
  - the live wish menu for query `bandage` lists `adhesive bandage` first and exact `bandage` second, so Package 5 is not disproved yet on the documented acceptance item
  - the blocker is now reliable exact-bandage restaging on the live path, not locker observability, pickup routing, or camp reassignment
- so Package 5 is still not honestly closed, but the remaining mismatch is finally narrow and explicit: either prove that true `bandage` stays in the kept lane on the same live packet, or consciously broaden/change the policy if `adhesive bandage` is supposed to count as part of the accepted bandages lane

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
