# Cannibal camp confidence-push live playtest imagination source of truth - 2026-04-28

Status: SOURCE OF TRUTH

Plan item: `Cannibal camp confidence-push live playtest packet v0`

## Finished scene

A normal-ish survivor near a cannibal camp does ordinary survivor things: makes smoke, makes noise, loiters, gets careless by day, gets too close by night, saves and reloads because games are games. The cannibals still feel like the behavior Josef asked for: vicious, opportunistic, night-favoring, and pack-minded, but not suicidal daytime goblins and not bandits with different hats.

This packet is not trying to invent new cannibal behavior. It is trying to make the already-finished cannibal night-raid slice harder to fool.

## What must feel true

- By day, smoke/routine/nearby activity creates pressure, scouting, stalking, or holding behavior rather than instant arcade combat.
- At night or under concealment/contact, the same broad family can become genuinely dangerous and pack-forward.
- Exposure matters: if the player spots or exposes the approach, cannibals hold, reposition, or abort instead of visibly beelining through common sense.
- Save/load does not lobotomize the active cannibal group, target, profile, or job state.
- Cannibals remain cannibals: no shakedown/pay/fight surface, no bandit toll cosplay.
- The result survives at least one less-blessed setup instead of only the McWilliams shrine.

## What would be hollow

- A tiny fixture pose proves the same line already proven by the closed matrix, but nothing broader.
- A live run reaches startup/load or debug setup, then never exercises real dispatch/local-contact behavior.
- The packet proves `profile=cannibal_camp` exists but not that the player-facing timing/pressure feels right.
- Repeatability only repeats the exact same save state and calls that diversity.
- Bandit/cannibal contrast is implied from code paths instead of shown through a small control.
- The test adds new behavior or tuning without first showing a confidence gap.

## Proof shape

The confidence push should be a small live playtest family, not a giant new feature lane:

1. **Wandering pressure:** player activity near a cannibal camp over bounded real time produces stalk/probe/hold pressure by day without instant suicide.
2. **Night mistake:** too-close night/concealed contact can turn into real pack attack through the live dispatch/local-contact path.
3. **Reload brain:** active cannibal group/profile/target/job state survives save/reload and continues coherently.
4. **Different-seed repeat:** at least one comparable setup with different camp position/member count/profile footing preserves the broad behavior.
5. **Bandit contrast control:** a similar contact shape still lets bandits use their shakedown/pay/fight surface while cannibals do not.

The proof should cite decisive step-ledger, report, saved-state, and log fields. Screenshots are useful only when a player-visible state is disputed.

## Review question

If Josef played for fifteen minutes near cannibals after this packet, would we expect the live behavior to feel like the proven matrix promised, or are we still trusting one neat laboratory terrarium?
