# Cannibal camp attack-not-extort correction v0 (2026-04-24)

Status: GREENLIT / next narrow correction candidate, not yet implemented.

## Why this exists

Josef clarified the intended cannibal-camp behavior after the first hostile-profile adopter packet closed:

> Cannibals do not extort. Cannibals attack to kill.

The existing `cannibal_camp` profile is real enough as a second hostile-site adopter: it has a dedicated rare camp anchor, cannibal NPC templates/classes, its own faction, explicit hostile-site profile, larger home reserve, and separate dispatch/writeback state.

But the local encounter gate is still mostly generic hostile-site logic. That means a cannibal-owned outing can still plausibly fall into the same `open_shakedown` / pay-or-fight surface that belongs to bandit robbery scenes.

That is wrong for cannibals.

## Scope

This correction should make cannibal camps use the shared hostile-site substrate while diverging at the local encounter surface:

- cannibal camp pressure should **not** open the bandit shakedown/extortion surface
- cannibal camp local contact should bias toward `attack_now`, lethal ambush, or stalking/probing until attack conditions are good
- if local threat is too high, cannibals may still hold off, stalk, probe, or abort according to the local gate, but the player-facing offer should not become `pay the demanded goods`
- reports/tests should make the profile distinction explicit: bandit camp can extort; cannibal camp attacks to kill / hunts, not negotiates
- keep the correction narrow: local gate/surface behavior only, not a giant cannibal society or diplomacy expansion

## Non-goals

- broad cannibal quest/lore systems
- capture/imprisonment/cooking-the-player mechanics in this packet
- full cannibal morale/social simulation
- rewriting bandit shakedown behavior
- making cannibals tactically perfect or always suicidal
- reopening the already-closed cannibal-camp anchor/profile proof

## Success state

This correction is done when:

- deterministic coverage proves a `cannibal_camp` active outing does not produce `open_shakedown` / pay-demand surface under the same broad conditions where a bandit camp could
- deterministic coverage proves the cannibal local-gate result is attack/stalk/probe/abort/hold-off as appropriate, with `attack_now` or equivalent lethal pressure when the scene is favorable
- report text or artifact output names the cannibal distinction clearly enough for review
- existing bandit shakedown/pay/fight tests still pass unchanged
- the cannibal-camp adopter packet stays closed as content/substrate proof; this correction only fixes the encounter-behavior semantic gap

## Testing expectations

Minimum validation:

- focused deterministic test for cannibal local-gate behavior
- existing focused bandit shakedown tests to prove bandit extortion was not broken
- `git diff --check`

Optional if cheap:

- one harness/report proof showing cannibal local contact does not surface a pay-demand menu
