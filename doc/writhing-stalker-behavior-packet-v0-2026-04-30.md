# Writhing stalker behavior packet v0

## Classification

Status: **GREENLIT implementation packet**.

Ledger ID: `CAOL-WRITHING-STALKER-v0`.

Imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`.

Testing/playtest ladder: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`.

Raw intake preserved at: `doc/writhing-stalker-raw-intake-2026-04-30.md`.

## Goal

Create the first playable writhing stalker v0: a rare singleton first-generation zombie-adjacent predator that uses coarse world interest, human/player evidence, light, cover, zombie distraction, and opportunity windows to stalk and strike without becoming omniscient or turning into a bandit-camp economy clone.

## Scope

Implement in bounded layers:

1. Monster definition and spawn footing.
2. Singleton/rarity guardrails.
3. A small stalker-interest/latch state model.
4. Coarse interest inputs from terrain/light/human evidence where already available or cheap.
5. Opportunity scoring for vulnerable/distracted player states.
6. Approach/hold/strike/withdraw decisions.
7. Save/load persistence for active latch/cooldown state if code state is added.
8. Deterministic tests for latch, opportunity, retreat, and no-omniscience behavior.
9. Live/harness playtests that prove an actual stalk-and-strike scene and at least one negative/control case.

## Non-goals

Do not include:

- bandit camp/site profile reuse as the main implementation;
- shakedown, faction economy, camp reserves, or structural harvesting;
- common spawn spam;
- teleport ambushes;
- exact global monster/loot scans;
- a full new faction system;
- long-term nemesis storytelling beyond a bounded latch/cooldown v0;
- broad overmap-interest refactor unless the slice truly needs a tiny shared adapter.

## Design contract

### Identity

The stalker is a singleton predator, not a squad. Its threat should be rare enough that seeing it feels like an event.

### Information

Allowed information:

- recent direct sight/sound/human marks already available to the game;
- exposed night light marks where existing light/signal code can cheaply expose them;
- coarse terrain/cover interest;
- nearby zombie pressure as opportunity;
- recent player vulnerability if already observable at the local AI seam.

Forbidden information:

- permanent exact player coordinates from nowhere;
- whole-map exact pathing every turn;
- exact inventory/loot scans;
- teleporting to an unseen player;
- refreshing fixation forever without new evidence.

### Behavior states

Recommended v0 state machine:

```text
latent / wandering
  -> interested       // broad scent/terrain/light/human clue
  -> latched          // believable player/human clue, leashed and timed
  -> shadowing        // approaches indirectly, prefers cover/darkness/clutter
  -> waiting          // holds until vulnerability/opportunity improves
  -> striking         // short cut/bleed/bite pressure
  -> withdrawing      // exposed/hurt/focused; creates cooldown
  -> cooling_off      // no immediate repeat unless evidence is refreshed
```

State names can change. The behavioral distinctions should not.

### Opportunity score

A strike becomes attractive when several of these are true:

- player is bleeding/hurt/low stamina;
- player is sleeping/resting/crafting/reading/otherwise committed;
- player is engaged with zombies or surrounded;
- player is noisy or recently made human evidence;
- player is near cover/clutter/town/forest edge;
- lighting favors the stalker;
- the stalker has a valid recent latch.

A strike should become unattractive when:

- the stalker is hurt;
- the player has direct focus/line of sight and space;
- bright exposure removes the ambush shape;
- latch age exceeds leash/cooldown;
- player has not refreshed evidence.

### Combat

Combat should be short and legible:

- fast approach or pounce only if it came from a valid shadowing/opportunity state;
- cut/bleed pressure matters;
- no long tank duel;
- withdrawal is part of the fantasy, not a failure.

## Implementation guidance

Recommended order:

1. Add monster JSON/stat/spawn-group v0 with intentionally rare/singleton footing.
2. Add deterministic AI decision helper(s) before clever live behavior.
3. Connect to a minimal live seam only after tests can describe why the stalker approaches/holds/strikes/withdraws.
4. Add harness fixtures and playtest probes only when the live seam exists.
5. Tune stats from live behavior, not from JSON admiration. Admiring JSON is how one gets a beautifully formatted mosquito.

## Evidence burden

A v0 cannot close from monster JSON alone.

Required proof types:

- JSON/schema validation for monster/spawn data.
- Deterministic decision tests for latch/opportunity/withdraw/no-omniscience.
- Save/load proof if latch state persists outside existing monster state.
- Live/harness proof for at least one believable stalk-and-strike path.
- Live/harness negative proof that no-stimulus/no-evidence scenes do not create instant omniscient pressure.
- A tuning readout that names whether it feels too common, too tanky, too fast, too invisible, too honest, or too stupid.

## Closure definition

Close `CAOL-WRITHING-STALKER-v0` only when:

- [ ] The monster/stat/spawn footing exists and validates.
- [ ] Spawn rarity/singleton rules are tested or otherwise proven.
- [ ] Stalker latch/opportunity/withdraw decisions have deterministic tests.
- [ ] Direct player/human evidence can create a bounded latch without omniscience.
- [ ] Weak/no evidence decays or fails to latch.
- [ ] Zombie pressure can increase opportunity without becoming a magic player locator.
- [ ] Strike behavior is short cut/bleed pressure rather than a tank duel.
- [ ] Withdrawal/cooldown prevents immediate repeat spam.
- [ ] Save/load preserves any new stalker state, or the packet explicitly avoids new persisted state.
- [ ] Live/harness proof shows a real stalk/hold/strike/withdraw scene from the game path.
- [ ] Live/harness negative/control proof shows no instant beeline with no valid evidence.
- [ ] `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and `doc/work-ledger.md` match the final closed/next state.

## Open tuning questions

These are greenlit for investigation, not pre-decided:

- Should the stalker be mostly night-biased or also dangerous in cluttered day conditions?
- Should withdrawal preserve the same individual for later return, or is v0 allowed to model only cooldown pressure?
- How much bleed is scary without becoming tedious bandage tax?
- How rare is rare enough that it feels special but not nonexistent?
- Should light lure it, repel it once close, or both depending on exposure?
