# Cannibal camp night-raid imagination source of truth (2026-04-28)

Status: SOURCE OF TRUTH / DRAFTED FROM JOSEF-SCHANI PRODUCT DIRECTION

Plan item: `Cannibal camp night-raid behavior packet v0`

## Finished scene

A cannibal camp is not a clever bandit toll booth with worse table manners.

It is a hungry hostile camp that already understands the same rough world as bandits — people travel, camps hoard food, smoke and light give sites away, targets can be too dangerous today and vulnerable later — but its decisions lean toward killing windows instead of leverage. Smoke and light should not be magic aggro buttons; they are smells on the wind that justify scouting, stalking, dispatch, and a later killing window.

The player should feel the difference before reading a debug log:

- bandits may scout, stalk, threaten, extort, or raid when the balance makes sense;
- cannibals may scout and stalk too, but their good outcome is an ambush, not a negotiation;
- if they commit, they prefer a pack that can actually drag the fight into terror, not one doomed fool jogging into daylight;
- while stalking, they should avoid being seen: use darkness, cover, distance, and broken line of sight; if exposed, reposition, hold off, or abort rather than stroll visibly forward like dinner delivering itself;
- darkness makes them bolder because it hides approach, confuses witnesses, and turns a camp/basecamp perimeter into a hunting ground;
- daylight, high alert, recent exposure, heavy defender strength, or no cover can delay them into watching, circling, or going home hungry.

The intended rhythm is: the cannibal camp smells human opportunity — smoke from a campfire, light in the wrong place, repeated human travel, a basecamp routine — then sends eyes toward it. The scouts try not to be seen. If they confirm prey worth eating, the camp dispatches a pack, lets it lurk outside vision, waits for night/darkness/concealment to make the approach favorable, and then the group attacks with the killing decision already made. No polite `pay the toll`, no single-cannibal suicide courier, no bandit-flavored shakedown wearing human jerky as a hat.

## Lived player experience

A player near a cannibal camp should not mainly think, “another bandit system fired.” They should think, “I should not have let that camp know we sleep here.”

Readable play shape:

- A cannibal camp is rarer and cruder than bandits, but more immediately lethal once it decides the player/basecamp is prey.
- Early signs can be similar to bandits — someone watching from far enough away, strange movement at dusk, pressure around smoke/light/travel routes — but the payoff is different.
- A bright camp, smoke column, or repeated lit routine can become a lead: not instant combat, but the beginning of being watched.
- They do not open with a commercial proposition. Contact is a hunting mistake, an ambush, a rush, or a stalking withdrawal until conditions improve.
- Night matters. If the player leaves smoke/light/camp routine exposed, the scary result is a nighttime pack attack or pressure event, not a lone cannibal politely feeding himself to the turret line.
- Seeing or wounding one of them should matter. A seen scout or failed attack can make the camp more cautious, angry, depleted, or return later with better timing, depending on surviving roster and confidence.

The feature is alive when the player can form superstition from it: “those people wait until dark.”

## System behavior from the outside

Cannibal camps should reuse the shared hostile-site/bandit-live-world substrate where that substrate is genuinely generic:

- owned hostile site records;
- profile-specific rules;
- per-site/camp memory;
- roster/home reserve/active group state;
- scout/stalk/raid dispatch shapes;
- local gate decisions;
- return/writeback pressure;
- save/load persistence;
- smoke/light/human opportunity marks where the camp-map ecology makes them available.

They should diverge at profile behavior:

- **No extortion surface.** Cannibal contact must not open bandit pay/fight shakedown.
- **Pack threshold.** Cannibal attacks should require a meaningful raiding group unless the target is already helpless. Ordinary planned attacks should not dispatch one cannibal as the whole fight.
- **Smoke/light scouting chain.** Smoke, exposed light, and repeated human/basecamp routine should be able to become cannibal leads. Those leads should first justify scouting or stalking; confirmed prey can then justify a dispatched pack, not instant teleporting combat from a raw signal.
- **Night/darkness preference.** The profile should strongly prefer attacking in darkness, night, poor visibility, or similarly concealed approach windows. Daylight should more often produce scout/stalk/probe/hold-off unless the target is exposed and weak.
- **Avoid-detection stalking.** Between lead confirmation and attack, cannibal scouts/stalkers should try to stay outside player/basecamp vision. Being seen should push bounded reposition, hold-off, or abort behavior unless the pack is already committing.
- **Trigger-happy once favorable.** When darkness/weakness/roster/confidence align, the cannibal profile should cross from stalk/probe into attack sooner than bandits would cross into shakedown/raid.
- **Harsher retreat/writeback.** Failed cannibal pressure should feed future caution or anger, but not magical infinite cannibals. Dead/wounded/away members reduce the next attack just like any other site.
- **Camp-map compatible.** Cannibals should be able to read the same kind of target knowledge as bandits, but interpret human/basecamp opportunity as prey more than loot/toll.

## Boundaries

This is not trying to create:

- a giant cannibal society/lore expansion;
- player capture/cooking mechanics;
- perfect stealth AI;
- teleporting attackers;
- suicidal always-attack behavior;
- a new separate global hostile-brain system;
- changes to ordinary bandit shakedowns;
- a singleton writhing-stalker design hiding inside camp work.

Cannibal camps stay camp-shaped. They are more vicious and less transactional, not a whole new genre of monster.

## Failure smells

- The code says `profile=cannibal_camp`, but the plan still dispatches one attacker because the generic bandit capacity rule happened to do that.
- Darkness is only a note string and never changes dispatch/local-gate outcome.
- Smoke/light signals make cannibals instantly attack without a scout/stalk/dispatch chain or any chance for detection/avoidance to matter.
- Cannibals “attack to kill” in deterministic local-gate tests, but live dispatch cadence never sends the group into the actual game path.
- The feature closes from a test that calls `choose_local_gate_posture()` directly while the running game still uses an old generic shakedown/attack seam.
- A camp sends a lone cannibal to attack a defended basecamp at noon because the code treats “trigger happy” as “stupid.”
- Night attacks spawn directly on the player or teleport through observed space instead of using the same honest local/overmap handoff constraints as bandit pressure.
- Stalking cannibals remain visible turn after turn without any sight-avoid, cover-seeking, reposition, hold-off, or abort behavior.
- Review treats “no shakedown” as full cannibal identity, when the missing part is the pack-at-night hunting behavior.

## Review questions

- If this shipped, would the player learn that smoke/light and repeated routine can get them watched before it gets them attacked?
- Would the player learn that cannibal camps wait for darkness, or only that an enum had a new branch?
- Does a cannibal attack involve enough living members to feel like a raid rather than a random encounter?
- Do stalking cannibals avoid player/basecamp vision, and do they reposition/hold/abort when exposed instead of continuing a visible approach?
- Are daylight, strong defenders, recent exposure, and failed scouts able to delay/redirect cannibals, or do they attack because the profile name is edgy?
- Does the proof reach the real lead/scout/dispatch/local-contact path, not only evaluator functions?
- Can bandits still extort while cannibals cannot, without the two profiles corrupting each other?
- Does save/load preserve the cannibal camp's roster, active group, target memory, and aftermath honestly?
- Is the result scary because the system chose a killing window, or merely because the text says “cannibal”?
