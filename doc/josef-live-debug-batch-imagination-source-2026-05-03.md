# Josef live debug batch imagination source — 2026-05-03

The finished scene is not a stack of clever subsystems. It is Josef loading his camp/base and seeing hostile pressure behave like it belongs in one world.

A bandit camp watches from a believable distance instead of sending one polite intern to stand on the doorstep and steal casings under three rifles. Scouts and stalking-mode bandits keep a real standoff distance, break sight when exposed, and report home. Being looked at matters; stalking is not allowed to be a visible garden-gnome pose with a different label. Where that stalking/avoidance/escalation logic is profile-neutral, it should be transposed instead of reimplemented as one-off bandit folklore. Cannibals get the same basic stalking discipline: watch from about `5` OMT where terrain permits, avoid being plainly seen, then if they believe they can overpower the defenders, send an attack dispatch — possibly a large/whole-camp commitment, not a shakedown. Their camps can also show why they are like this: one or two huge, sturdy Monsterbone spears made from impossible monster remains, ritual/status weapons carried only by important people, the kind of bone that says somebody ate meat from a thing large enough to be a dungeon and came back wrong. A large bandit camp that has already confirmed a valuable base escalates with muscle: a toll/shakedown party, not another lone clipboard scout. If the player is on a roof, the camp does not split into two ghost camps by z-level or try to overmap-route ground bandits to a roof tile and then sulk for thirty minutes.

When the shakedown opens, the player sees one hard bandit fork: Pay or Fight. Refusing is fighting; it is not a third piece of UI bookkeeping. Paying opens a trade/debt-style surface against the honest scene pool: carried goods, basecamp goods, nearby faction/basecamp inventory, and basecamp NPC carried items. The player chooses what to surrender. The game does not silently rummage through pockets like a goblin clerk.

Light is world evidence. Fire, lamps, windows, searchlights, and other meaningful exposed light sources can matter depending on brightness, weather, containment, time of day, and direction. Bandits read light with some intelligence: a searchlight looks like active defense, while household light or fire looks like occupancy, bounty, or camp life. Zombies are simpler. To them, exposed light is attraction pressure, whether it comes from fire or a lamp.

Basecamp defenders also stop behaving like passive scenery. Patrols attack hostile bandits and zombies on sight while leaving neutrals/unknowns alone. A hostile sighting can alarm the camp: anyone with patrol responsibility may be pulled onto the patrol roster independent of the current shift. Routine patrol route bookkeeping stays out of the player-facing message stream unless it is actionable.

The writhing stalker remains cautious under high threat, but cautious means distant, hidden, and actually out of sight — not merely renamed loitering. Its stalking distance needs to be larger than the prior close proof made comfortable, and its sight avoidance has to visibly work. When the threat window opens — zombies distract defenders, line pressure drops, the player becomes vulnerable — it swoops in quickly. It should feel predatory, not bureaucratically delayed. After the short strike/burst, it boots itself back out into cover/stalking quickly enough that the rhythm reads as hit-fade-hunt, not stand-there-and-think-about-it.

NPC sorting failures are boring in the right way. If a worker cannot sort because the setup is impossible, full, unreachable, or too heavy, they record the failure and cool down. They do not retry the same impossible job every turn like a tiny machine designed to annoy log files.

Debug spawning also becomes useful instead of ornamental. “Spawn horde” should make a medium horde worth testing against, not a sad bookkeeping pebble. Josef can stage pressure at named overmap distances — especially `5` and `10` OMT — and can also stage writhing stalker and zombie rider encounters at those distances without rebuilding a save by hand every time.

## Failure smells

- A closed proof says Pay/Fight/Refuse is green after Josef already reported that three choices were wrong.
- A proof asserts shakedown payment while no trade/debt/choice surface ever opened.
- A multi-z camp appears as multiple camps with duplicated schedulers/headcounts.
- Roof or z-level player position makes overmap routing fail and then throttles response into silence.
- A scout is visibly looked at near a defended camp and neither withdraws nor escalates.
- Ordinary lamps/windows are ignored while fire-only signals are treated as the whole light system.
- Zombies distinguish between fire-light and lamp-light as if they were filing categories.
- Patrols report routine loops to the visible message log during waiting.
- Sorting retries repeat per-turn/per-log-tick after a known failure.

## Review questions

- Does each fix reach the real live path, not only a deterministic seam?
- Does each scenario separate no-signal, ordinary light/fire, searchlight/threat, high-threat defended camp, and actual shakedown contact?
- Are old misleading green claims corrected in canon so future work does not inherit the wrong contract?
- Are non-bugs classified as such, especially horde deletion that already proved local removal?
