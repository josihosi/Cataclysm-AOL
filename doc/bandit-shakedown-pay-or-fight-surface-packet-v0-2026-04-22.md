# Bandit shakedown pay-or-fight surface packet v0 (2026-04-22)

Status: greenlit backlog contract.

## Why this exists

Once the approach law can honestly decide that a scene is a shakedown instead of a hot ambush, the next missing seam is the actual player-present robbery surface.
Without this, the bandit pressure chain still stops right before the point where the player experiences it as a real game event.

This packet exists to make that scene concrete and readable:
not a diplomacy opera, just one hard pressure fork where bandits try to rob you and you can either pay or fight.

## Scope

1. Land one honest bootstrap from the bandit approach/gate layer into a player-present shakedown scene.
2. Start the encounter with a short readable bark/intent surface that makes it obvious this is a stick-up rather than a generic hostile aggro flip.
3. Expose one deliberately blunt first fork: `pay` or `fight`.
4. Keep that hard fork available whenever this shakedown surface is invoked; if a later packet reopens pressure with a harsher second demand, the player should still be able to answer with `pay` or `fight` rather than losing the combat option by script fiat.
5. Use a trading-style surrender surface rather than pure freeform bark parsing, so the extortion scene is usable and reviewer-readable.
6. Make the demanded toll painful by rule: a meaningful share of the honest reachable goods pool, not a token scripted fee.
7. Keep the reachable goods pool explicit:
   - Basecamp-side scenes may expose player inventory, relevant nearby NPC inventories, and reachable Basecamp inventory
   - off-base scenes may expose only what the player, companions, and current vehicle actually carry
8. Allow surrendered goods to collapse into abstract bandit bounty/writeback instead of requiring perfect item-by-item future cargo theater.
9. Keep convoy / fast-moving rolling-attack contexts allowed to bypass this packet when packet 1 already decided the honest posture is direct violence.

## Non-goals

- branching negotiation theater with ten diplomacy states
- magical access to remote inventory outside the honest current scene
- a fake debt economy that silently reaches into unrelated future loot
- radio-driven information war, writhing-stalker pressure, or social-camouflage systems
- full hostage, prisoner, or reputation simulation
- convoy scenes that should honestly just open with violence

## Success shape

This packet is good enough when:
1. one honest player-present shakedown scene can bootstrap from the prior approach/gate packet instead of appearing as disconnected chat magic
2. the initial interaction is clearly a robbery demand with a readable `pay` versus `fight` fork
3. whenever this shakedown surface is invoked, including any later reopened demand, fighting remains an explicit option rather than disappearing into one-way surrender theater
4. the surrender surface uses an explicit bounded goods pool that matches scene context rather than magical remote inventory reach
5. the demanded toll is painful enough to read like a real bandit shakedown instead of a decorative nuisance fee
6. paying can resolve the immediate scene without requiring perfect long-tail cargo simulation, because surrendered goods can collapse into bandit bounty/writeback honestly
7. the slice stays bounded and does not widen into a giant diplomacy system, fake debt economy, or unrelated convoy-combat rewrite

## Validation expectations

- prefer live proof on at least one Basecamp-side scene and one off-base exposed-travel scene where the packet honestly applies
- reviewer-readable output should show the trigger context, the demanded pool, and the chosen branch clearly enough that later tuning notes are about feel rather than `what happened`
- if one short bark family is introduced, keep it bounded and readable instead of hiding core logic behind flavor spam
- validation should prove that contexts explicitly ruled as direct ambushes are still allowed to skip this packet cleanly

## Current grounded note

The broader parked concept footing already leans toward exactly this blunt surface: explicit `pay` versus `fight`, a trading-style window, painful tolls, Basecamp scenes exposing nearby + camp inventory, off-base scenes exposing only actually carried goods, and surrendered goods collapsing into abstract bounty.
This packet exists to promote that lean into bounded canon rather than leaving it as attractive background muttering.

## Dependency note

This queued packet sits directly behind `doc/bandit-approach-stand-off-attack-gate-packet-v0-2026-04-22.md`.
Radio warfare, `writhing stalker`, and the broader Arsenic-and-Old-Lace social-threat bank stay parked for later.
