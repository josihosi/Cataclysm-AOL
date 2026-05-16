# Bandit aftermath / renegotiation writeback packet v0 (2026-04-22)

Status: active / greenlit contract.

## Why this exists

A shakedown scene is fake if the world forgets it immediately afterward.
The point is not only that bandits can demand payment once.
The point is that the outcome should matter:
who died, who got hurt, what was taken, how much threat dropped, whether the bandits got bloodied, and whether the next pressure pass should come back meaner or more cautious.

This packet exists to freeze that aftermath law, including the specific wanted beat where killing a Basecamp defender can let bandits reopen the pressure from a stronger position with a higher demand instead of pretending the fight changed nothing.

## Scope

1. Land one honest aftermath/writeback layer for player-present bandit shakedown scenes.
2. Write back the concrete result of the scene into later bandit and target state: losses, wounds, taken goods, failed extraction, anger, increased caution, or stronger greed.
3. Let partial or interrupted fights rewrite the next pressure pass honestly instead of forcing every scene to resolve as one clean terminal event.
4. Allow one bounded renegotiation reopen when the local fight materially lowers defender strength, especially if a Basecamp NPC dies and the bandits now read the scene as richer/safer prey than a minute ago.
5. Make that reopened demand explicitly harsher rather than just replaying the same toll as if nothing changed.
6. Give the player one honest chance to reconsider under that higher price: pay the raised demand or fight again. The combat option should remain real on both the first and second demand.
7. Keep the reopen logic bounded: one honest stronger second demand is good; an infinite haggle/loop machine is not.
8. Preserve both sides of the writeback truth: bandit losses or panic should cool later pressure too, not only player/basecamp losses increasing future extortion.

## Non-goals

- infinite renegotiation loops or endless haggling AI
- broad camp-morale opera, faction diplomacy, or reputation simulation
- magical item-perfect cargo persistence for every surrendered object
- multi-camp retaliatory coalition logic
- radio escalation, writhing-stalker pressure, or wider social-horror systems
- retroactively rewriting the earlier attack-gate or pay-or-fight packets into one giant mega-fix

## Success shape

This packet is good enough when:
1. one honest aftermath/writeback layer exists for player-present bandit shakedown outcomes
2. later bandit behavior can reflect scene results such as losses, wounds, loot haul, failed extraction, anger, or caution instead of resetting to folklore
3. a materially weakened Basecamp defense, including a killed defender, can reopen the pressure once from a stronger bandit position with a higher demand when that is the honest read
4. that harsher reopened demand still gives the player a fresh explicit `pay` versus `fight` choice instead of hard-forcing only surrender or only combat
5. the harsher reopened demand is explicit and reviewer-readable rather than hidden in vague score drift
6. bandit losses or panic can also cool or shrink later pressure, so the packet does not only ratchet in one cruel direction
7. the slice stays bounded and does not turn into infinite haggling, giant diplomacy machinery, or coalition grand strategy

## Validation expectations

- prefer proof that a first encounter outcome actually changes a later revisit or immediate reopen, rather than stopping at one isolated scene transcript
- include at minimum one case where bandits return stronger after defender loss and one case where bandits cool or shrink after taking meaningful losses themselves
- when the stronger return happens, validation should prove the player still gets a fresh `pay` versus `fight` reconsideration fork under the raised demand
- reviewer-readable output should show the scene outcome, the rewritten local threat read, and the resulting higher-or-lower later demand clearly enough that the change can be audited by reading
- if goods are abstracted into bounty/writeback, validation should prove that later pressure changes are still grounded in the stored outcome rather than fake amnesia

## Current grounded note

The broader parked bandit concept already leaned toward this exact follow-through: if a player-present fight kills defenders or materially lowers local Basecamp threat, a later pressure pass may reopen negotiations from a stronger bandit position with a higher toll instead of pretending nothing changed.
This packet now also freezes Josef's later correction that the reopened demand should still offer a real reconsideration fork: pay the raised price or fight again.

## Dependency note

This queued packet sits directly behind `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md`.
Radio warfare, `writhing stalker`, and the broader Arsenic-and-Old-Lace social-threat bank stay parked for later.
