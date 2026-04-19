# Bandit concept formalization follow-through work packages and micro-items (2026-04-19)

This document greenlights a **bottom-of-stack conceptualization pass** for the parked bandit packet.

The point is not to start coding bandit AI.
The point is to stop leaving the remaining control law as muttered vibes, stale "decay" wording, and scattered half-rules across several parked docs.

This follow-through is intentionally decomposed into **small single-question spec items** so Andi can pressure-test one law at a time instead of smuggling five assumptions into one nice-sounding paragraph.

## Operating rule

- **One package at a time.**
- **One micro-item at a time inside that package.**
- This is **doc/spec work only**, not implementation.
- Keep it at the **bottom of the greenlit pile** until the current locker/basecamp fixes and the already-greenlit backlog above it are honestly done or deliberately skipped.
- Use the existing parked bandit docs as substrate, but correct them where they still smuggle in wrong passive-decay assumptions.
- If a micro-item reveals a real unresolved design fork, stop and report instead of inventing fake certainty.
- Do not silently answer multiple micro-items at once just because the laws smell related.

## Frozen footing

These rules are already decided for this follow-through:

- there is **no passive decay** for bounty
- there is **no passive decay** for threat
- structural bounty is set by terrain / tile class and reduced by harvesting or exploitation
- moving bounty changes when real actors or visible activity change
- threat changes when bandits actually recheck / observe something and rewrite their read
- bandit camp stockpile may go down by explicit consumption, and any future spoilage rule must stay explicit and separate from bounty/threat logic

## Inputs already available

Use these as the current substrate instead of restarting from zero:

- `doc/bandit-overmap-ai-concept-2026-04-19.md`
- `doc/bandit-bounty-threat-scoring-guidance-2026-04-19.md`
- `doc/bandit-mark-generation-and-heatmap-model-2026-04-19.md`
- `doc/bandit-overmap-to-bubble-handoff-seam-2026-04-19.md`
- `doc/bandit-player-basecamp-visibility-and-concealment-2026-04-19.md`
- `doc/bandit-visibility-physical-systems-recon-2026-04-19.md`

---

## Package 1. Bounty source, harvesting, and camp-stockpile rules

Resolve these as **separate micro-items**. Do not treat "resource law" as one blob.

### A. Structural bounty law

#### 1. Terrain bounty bucket table
- **Question:** Which terrain / site classes carry base structural bounty?
- **Expected output:** One starter table covering at least open street, field, forest, cabin, house, farm, city structure, and camp footprint.
- **Done when:** The table makes the current lean explicit, fields/open streets near-zero, forests low but non-zero, buildings materially higher where intended.

#### 2. Structural bounty harvest trigger rule
- **Question:** What exact bandit action counts as harvesting or exploiting structural bounty?
- **Expected output:** One rule note that says what must happen before a site loses some or all structural bounty.
- **Done when:** Later implementation will not have to guess whether mere visitation, scouting, looting, raid completion, or occupation caused the depletion.

#### 3. Structural bounty reappearance rule
- **Question:** Under what real-world changes, if any, can structural bounty become worth considering again?
- **Expected output:** One explicit rule for whether structural bounty can return, and by what world change rather than by passive timer.
- **Done when:** The packet no longer hand-waves about reset or drift.

### B. Moving bounty law

#### 4. Moving bounty source table
- **Question:** Which live signals create moving bounty?
- **Expected output:** One source table covering humans, NPC traffic, basecamp routine, caravans, smoke, light, sound-derived contact clues, and similar activity.
- **Done when:** Each source is named clearly enough that later mark generation can point to a real category instead of vibes.

#### 5. Moving bounty attachment rule
- **Question:** Is moving bounty attached to an actor, a route segment, a site-centered mark, or some bounded mixture?
- **Expected output:** One narrow rule describing where moving bounty lives in the abstract model.
- **Done when:** The packet no longer blurs together actor bounty and ground bounty.

#### 6. Moving bounty clear / rewrite rule
- **Question:** When the source changes or disappears, how is moving bounty rewritten, cleared, or replaced?
- **Expected output:** One rule for source-gone, source-moved, and source-confirmed cases.
- **Done when:** Later implementation does not need to invent fake "soft decay" language to clean up stale moving signals.

### C. Threat law

#### 7. Threat source table
- **Question:** Which conditions create threat marks in the first place?
- **Expected output:** One source table covering zombies, defenders, fortification signs, recent losses, gunfire, failed probes, and similar causes.
- **Done when:** Threat sources are explicit enough that the later score law can point to concrete inputs.

#### 8. Threat rewrite rule
- **Question:** What kinds of recheck or observation can raise, confirm, or lower threat?
- **Expected output:** One explicit rewrite rule that replaces any lingering passive-threat language.
- **Done when:** The packet plainly says threat only changes through real recheck, not idle clock shave.

#### 9. Threat-and-bounty coexistence rule
- **Question:** How can one region be both attractive and dangerous at the same time?
- **Expected output:** One rule explaining coexistence instead of silent overwrite.
- **Done when:** The model no longer implies that threat deletes bounty or bounty deletes threat.

### D. Harvesting and stockpile law

#### 10. Destination-only vs along-route collection rule
- **Question:** Can bandits collect value only at the explicit destination, or also opportunistically while traveling?
- **Expected output:** One bounded route-collection rule.
- **Done when:** Later logic will not secretly oscillate between destination-only and vacuum-cleaner route harvesting.

#### 11. Collection yield rule
- **Question:** How much harvestable bounty can one outing convert into haul or stockpile?
- **Expected output:** One starter yield rule or small table.
- **Done when:** Later stockpile math has a real ingress rule instead of hand-wavy "some amount".

#### 12. Camp stockpile consumption rule
- **Question:** What drains camp stockpile, and on what explicit cadence family?
- **Expected output:** One rule packet for stockpile consumption, separate from bounty/threat logic.
- **Done when:** The packet states plainly that stockpile goes down by consumption rather than by hidden bounty-style decay.

#### 13. Forest yield rule
- **Question:** What practical value can bandits actually extract from forest, separate from generic low forest bounty?
- **Expected output:** One small forest-yield note with starter numbers or bounded categories.
- **Done when:** "Forest has a little bounty" and "forest can actually feed stockpile a bit" are separated cleanly.

### Package 1 acceptance bar
- all 13 micro-items above exist as separate answers or tables
- structural vs moving bounty is no longer blurred
- harvesting, collection, and consumption language fully replaces passive-decay slop
- later implementation will not need to invent the basic resource law from scratch

### Out of scope for Package 1
- coding the system
- per-bandit inventory simulation
- full faction-economy balance for the whole game

---

## Package 2. Cadence, distance burden, and route-fallback rules

Resolve these as **separate movement/control-law micro-items**.

### E. Movement budget and cadence law

#### 14. Daily movement budget rule
- **Question:** What is the starter daily OMT travel budget for relevant bandit outing types?
- **Expected output:** One small starter table.
- **Done when:** The packet no longer relies on vague phrases like "they can move a few tiles".

#### 15. Cadence budget-spend rule
- **Question:** How do cadence wakes spend existing budget without minting fresh travel budget every pass?
- **Expected output:** One explicit cadence-spend rule.
- **Done when:** The concept cannot accidentally teleport groups across the map just because the AI woke often.

#### 16. Distance burden rule
- **Question:** How does target desirability fall off with travel distance and return burden?
- **Expected output:** One discount rule or starter table.
- **Done when:** Range pressure is concrete enough to compare two targets honestly.

#### 17. Return-clock rule
- **Question:** How long can a group stay out before it should prefer turning home?
- **Expected output:** One return-clock rule with starter values.
- **Done when:** The packet no longer implies immortal stalking squads that stay out forever.

#### 18. Cargo / wounds / panic burden rule
- **Question:** Which burden factors shorten outings or reduce useful movement?
- **Expected output:** One bounded penalty table or rule note.
- **Done when:** The concept has explicit burden pressure beyond raw distance.

### F. Fallback and diversion law

#### 19. No-target fallback rule
- **Question:** What does a camp do when nothing nearby looks worthwhile?
- **Expected output:** One explicit idle / low-value fallback behavior.
- **Done when:** The system will not inherit current random-NPC wander nonsense by accident.

#### 20. No-path fallback rule
- **Question:** What happens when a target exists but has no sensible reachable route?
- **Expected output:** One explicit no-path rule.
- **Done when:** The packet no longer leaves unreachable targets to implicit engine behavior.

#### 21. Mid-route abort / divert / shadow rule
- **Question:** What can make a group abandon, shadow, probe, or switch away from its original plan mid-route?
- **Expected output:** One bounded diversion rule.
- **Done when:** Mission leads stop looking like sacred tile commitments.

### Package 2 acceptance bar
- all 8 micro-items above exist as separate answers or tables
- movement budget, cadence, distance burden, and return pressure are explicit enough to compare routes honestly
- no-target and no-path behavior is implementation-legible
- the packet does not use passive decay as a crutch for movement cleanup

### Out of scope for Package 2
- replacing existing pathfinding code right now
- exact micro-path cost formulas for every overmap terrain
- local bubble combat behavior

---

## Package 3. Cross-layer interaction packet with starter numbers and worked scenarios

Resolve these as **separate scoring/seam sanity micro-items**.

### G. Job generation and scoring law

#### 22. Job candidate generation rule
- **Question:** What job candidates even get onto the board for a camp to consider?
- **Expected output:** One candidate-generation rule note.
- **Done when:** Later scoring is not secretly deciding eligibility and desirability in one muddy pass.

#### 23. Job scoring formula shape
- **Question:** What are the main weighted factors in job scoring?
- **Expected output:** One explicit formula shape or score table, even if some numbers remain provisional.
- **Done when:** The packet names the factors instead of hiding them in prose.

#### 24. Need-pressure override rule
- **Question:** When does camp need or low stockpile make a mediocre opportunity worth taking anyway?
- **Expected output:** One bounded override rule.
- **Done when:** The model can explain desperate behavior without pretending bandits became stupid.

#### 25. Threat veto vs soft-veto rule
- **Question:** When does threat fully block a job, and when does it merely discount it?
- **Expected output:** One explicit veto ladder.
- **Done when:** The concept no longer hand-waves the difference between "dangerous but worth it" and "absolutely not".

### H. Handoff and persistence law

#### 26. Overmap-to-bubble entry-mode chooser
- **Question:** Which high-level mode should a group enter local play in, and what decides that?
- **Expected output:** One small entry-mode chooser packet covering at least scout, probe, harvest, ambush, raid, shadow, and withdrawal-style cases.
- **Done when:** The handoff seam has explicit mode selection instead of generic spawn-and-vibes.

#### 27. Bubble-to-overmap return-state packet
- **Question:** Which exact fields must come back from bubble play?
- **Expected output:** One return-state field list.
- **Done when:** Cargo, wounds, losses, morale/panic, mark changes, and stockpile delta are named concretely enough to serialize later.

#### 28. Save/load persistence boundary
- **Question:** Which bandit-side state must survive save/load and which may stay abstract?
- **Expected output:** One persistence-boundary note.
- **Done when:** The concept has a real schema boundary instead of a vague "save all the important stuff" gesture.

### I. Sanity-check packet

#### 29. Starter numbers table
- **Question:** What compact starter number sheet should the whole packet use consistently?
- **Expected output:** One cross-cutting table covering the still-open control-law numbers.
- **Done when:** The packet stops scattering provisional numbers across prose.

#### 30. Worked scenarios packet
- **Question:** Do the combined laws still make sense when pushed through concrete situations?
- **Expected output:** At least 8 worked scenarios.
- **Done when:** The packet includes cases such as forest-adjacent low structural bounty with visible activity, harvested houses, scary city edges, long-distance tempting targets, unreachable targets, moving-activity trails, and stockpile-pressure decisions.

#### 31. Invariants and non-goals packet
- **Question:** What must never happen, and what is explicitly out of scope for v1?
- **Expected output:** One invariants/non-goals sheet.
- **Done when:** Later implementation and review can spot nonsense immediately instead of arguing from vibes.

### Package 3 acceptance bar
- all 10 micro-items above exist as separate answers or tables
- starter numbers, worked scenarios, and invariants are explicit enough to pressure-test the whole control law
- stale passive-decay wording is corrected or explicitly flagged in the parked docs
- the result makes the whole bandit packet more implementation-legible without silently greenlighting implementation

### Out of scope for Package 3
- implementation
- balancing every edge case in one pass
- reopening the whole bandit concept from scratch

---

## What done looks like for the whole follow-through

This follow-through is done when:
- the 3 packages above exist as explicit doc/spec slices
- the **31 micro-items** above each have a narrow answer, table, or rule note
- the no-passive-decay footing is normalized across the packet
- later implementation planning can pick the packet up without rediscovering the control law from scratch

If that happens, the parked bandit chain is cleaner.
It is still not automatic permission to code the whole thing. One bureaucratic indignity at a time.
