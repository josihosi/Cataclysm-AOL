# Writhing stalker live fun-scenario imagination source of truth — 2026-04-30

## Finished scene

The player does not meet the writhing stalker as a spreadsheet with claws. They meet it as a pressure story: a thing that seems to know where the dark lanes are, waits when the light is wrong for it, cuts in when the player is tired or distracted, and then slips away when badly injured. The player should think, “that was unfairly creepy,” not “the game cheated.”

The important emotion is fair dread. The stalker is dangerous because the player’s situation creates openings: bleeding, low stamina, zombies, noise, poor light, bad footing, broken attention. The player also has tools: light, distance, doors, focus, cover control, and hurting the creature enough to make it retreat.

The best little story is: I saw hints, I made choices, it punished a bad moment, I recovered or drove it off, and afterward I understood why it happened. If the only story is “invisible ankle goblin teleported into my death spiral,” the scenario failed, even if the unit tests are green and the log looks smug.

## Outside-visible behavior

A live scenario packet should show five player-facing moods:

1. **Safety has rules:** no evidence means no magic beeline, and bright/focused safety makes it hesitate.
2. **Dark routes matter:** cover/edge approaches produce stalking, not open sprinting.
3. **Vulnerability matters:** being hurt, bleeding, tired, noisy, or distracted opens strike windows.
4. **Pressure breathes:** a strike is followed by cooldown/reposition, not blender spam.
5. **The monster wants to live:** once badly injured, it withdraws even if the player is still vulnerable.

## Product boundary

This is a live/harness fun-scenario proof packet, not a redesign of writhing-stalker AI. It may expose a tuning need, but it should start by proving or falsifying the current behavior in live-feeling scenes.

This is also not a demand for a full manual playtest. Harness evidence is allowed, but every credited row must connect to player-facing facts: visible/saved state, live-plan lines, turn/wait evidence, monster/player positions, decisions/reasons, strike/retreat timing, and stability.

## Failure smells

- No evidence, yet the stalker acquires the player and closes anyway.
- Campfire/light/focus does not matter.
- The creature chooses an open beeline while a cover/edge route exists.
- Zombie pressure magically grants strike permission without player vulnerability or evidence.
- The first strike ends the hunt forever while the stalker is healthy and the player remains vulnerable.
- The stalker repeats strikes with no cooldown/reposition breath.
- The stalker fights to the death after being badly hurt.
- The player has no readable escape or counterplay path.
- The proof is just a load/run artifact with no scenario-local pass/fail facts.

## Review questions

- Would a human player learn useful rules from the scene?
- Does the scenario feel tense because the player made a dangerous situation, not because the AI cheated?
- Is there at least one clear “I can counter this” lever?
- Does the evidence show a behavior arc, not merely a single decision line?
