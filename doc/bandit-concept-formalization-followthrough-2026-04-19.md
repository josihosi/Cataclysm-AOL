# Bandit concept formalization follow-through work packages (2026-04-19)

This document greenlights a **bottom-of-stack conceptualization pass** for the parked bandit packet.

The point is not to start coding bandit AI.
The point is to stop leaving the remaining control law as muttered vibes, stale "decay" wording, and scattered half-rules across several parked docs.

## Operating rule

- **One package at a time.**
- This is **doc/spec work only**, not implementation.
- Keep it at the **bottom of the greenlit pile** until the current locker/basecamp fixes and the already-greenlit backlog above it are honestly done or deliberately skipped.
- Use the existing parked bandit docs as substrate, but correct them where they still smuggle in wrong passive-decay assumptions.
- If a package reveals a real unresolved design fork, stop and report instead of inventing fake certainty.

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

### Problem
The parked bandit packet now has the broad shape, but the resource side is still too hand-wavy.
It needs explicit non-passive rules for where bounty comes from, how bandits collect it, how forest/field/building classes differ, and how camp stockpile rises or falls.

### Deliverable
Produce one clean packet that defines:
1. structural bounty by terrain / tile-class bucket
2. moving bounty sources from humans, camps, traffic, smoke, light, and similar activity
3. what it means for bandits to **harvest** or **exploit** bounty
4. whether collection happens only at the destination, or can also happen along a route under bounded rules
5. what enters camp stockpile, and how stockpile later goes down by consumption
6. where older parked docs still say or imply "decay," and what the corrected harvesting / collection / consumption language should be instead

### Acceptance bar
- explicit terrain/source buckets exist, including the already-agreed lean that fields/open streets are near-zero structural bounty and forests are low but non-zero
- harvesting/collection rules are stated concretely enough that later implementation will not have to invent them on the fly
- camp stockpile growth and consumption are explicit enough that later numbers work has a real substrate
- the packet states plainly that bounty does **not** passively decay

### Out of scope
- coding the system
- per-bandit inventory simulation
- full economy balancing for every faction in the game

---

## Package 2. Cadence, distance burden, and route-fallback rules

### Problem
The concept packet has useful starter language about cadence and daily movement budget, but it still lacks one clean control-law packet for how cadence, distance, return clocks, route collection, cargo burden, and no-target fallback actually interact.

### Deliverable
Produce one clean packet that defines:
1. cadence tiers vs daily movement budget, without turning frequent cadence into teleport nonsense
2. distance burden and return-clock pressure
3. cargo, wounds, panic, morale, or loss penalties that shorten outings or reduce useful travel
4. whether bandits may opportunistically collect bounty while already traveling
5. what happens when nothing promising is nearby
6. what happens when a goal exists but no valid path / reachable approach exists
7. explicit fallback behavior so bandits do not inherit current random-NPC wander nonsense by accident

### Acceptance bar
- starter numbers exist for movement budget, cadence interaction, and distance burden
- no-target and no-path fallback behavior is explicit and implementation-legible
- route-collection rules are concrete enough that later scoring/handoff work can rely on them
- the packet does **not** rely on passive decay to solve routing or pressure problems

### Out of scope
- replacing existing pathfinding code right now
- exact micro-path cost formulas for every overmap terrain
- local bubble combat behavior

---

## Package 3. Cross-layer interaction packet with starter numbers and worked scenarios

### Problem
The remaining bandit questions now mostly live in the seams between layers: bounty source, threat memory, visibility, cadence, harvesting, stockpile pressure, route choice, and return/handoff consequences.
If those seams stay scattered, Andi will improvise private mathematics later and the whole thing turns to soup again.

### Deliverable
Produce one packet that:
1. gives a compact starter numbers table for the still-open control-law pieces
2. walks through worked scenarios that combine the major layers honestly
3. names invariants, non-goals, and known traps
4. patches stale wording in the parked docs where "decay" still appears as a fake solution
5. leaves the whole bandit packet more implementation-legible without greenlighting implementation itself

### Minimum worked scenarios
Include enough scenarios to stress the seams, for example:
- forest-adjacent low structural bounty vs visible activity
- harvested house cluster that stays structurally depleted
- scary city edge with high threat and bounded opportunism
- long-distance target that looks rich but loses on distance burden / return clock
- target with promising bounty but no clean reachable route
- moving-activity trail that raises interest without magical omniscience
- camp pressure case where stockpile need matters more than raw greed

### Acceptance bar
- one starter numbers table exists for the open control-law pieces that remain after Packages 1 and 2
- at least 5 grounded worked scenarios exist
- invariants/non-goals are explicit enough to stop later scope creep
- stale passive-decay wording is corrected or explicitly flagged for correction in the parked bandit docs

### Out of scope
- implementation
- balancing every edge case in one pass
- reopening the whole bandit concept from scratch

---

## What done looks like for the whole follow-through

This follow-through is done when:
- the three packages above exist as explicit doc/spec slices
- the no-passive-decay footing is normalized across the packet
- the remaining cross-layer rules have starter numbers and worked scenarios instead of hand-waving
- later implementation planning can pick the packet up without rediscovering the control law from scratch

If that happens, the parked bandit chain is cleaner.
It is still not automatic permission to code the whole thing. One bureaucratic indignity at a time.
