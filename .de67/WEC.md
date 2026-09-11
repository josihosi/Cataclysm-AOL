# WEC — Zombies and light

*User intent and action plan; accepted discussion brief for de67 2.*

## Owner addendum authorising phase 2

Josef's latest request: "alright! de67 2. one addendum: we are working on mac mini caol dev worktree, make the de67 2 agent native in codex over there. there is already an FS, it is worked off, so the agent can archive that first and create a new one. make sur ethe FS is specific, like the word says, it should really read descibe the main functions in a very mechanistic way that a lesser luna agent can hardly mess up."

The native phase owner works directly in `/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL-hostile-ecology-dev` on the Mac mini, branch `dev`. At handoff, HEAD was `77ef2445bf30a274dd13cef60847eb08d7cd9005`; recheck current state. Existing unrelated product, runner, harness and documentation edits are user-owned and must remain intact.

Josef explicitly authorises archiving the completed prior FS before creating the new one. Preserve the exact current FS, its DFS compatibility entrypoint if present, and its associated old WEC as a recoverable, clearly labelled historical specification set before importing this new WEC. Archive only the superseded specification material needed for this replacement, not the whole `.de67` tree, product files, live state, or evidence history. Verify the archive and retain traceability before replacing active files. Existing acceptance/history is historical evidence, not credit for new requirements. Do not silently overwrite a different WEC: this addendum is the explicit owner resolution to preserve the old brief with its old FS and then import this new brief. If another live owner is still writing the old specification, do not race it or kill unrelated work; report the concrete conflict.

Write the FS for a Luna implementer: describe the affected main functions mechanistically, with actual file/symbol anchors, inputs and outputs, coordinate spaces and units, preconditions, branch/transition precedence, authoritative state owner, caller/callee handoffs, side effects, failure/blocked-path behavior, time advancement, persistence/migration and exact tests. Explain why the chosen mechanism works and which tempting incorrect implementation it rules out. Label proposed new symbols as proposed. Resolve technical design details from code; do not leave core behavior as "handle appropriately" or an unchosen menu of alternatives. Preserve owner-level open choices instead of quietly changing intent. Use pseudocode where it genuinely removes ambiguity, without turning the document into brittle implementation ceremony. Quality and clarity matter more than filling template fields.

This is phase 2 only. Do not implement gameplay changes, edit product tests, launch new game playtests, or start de67 3. Prepare the mechanistic specification and future playtests. Phase-2-native setup/probes/checkpointing follow the selected phase skill and must preserve unrelated dirty changes and existing history. Use the installed FS/DFS canonical naming contract consistently, rather than creating competing full specifications.

## Intended outcome

C-AOL should deliver three connected experiences:

1. **Writhing stalker:** an early-game, relatively common, weak but opportunistic predator. It follows a developing situation and attacks when the player becomes vulnerable—including in daylight when enough zombies are pressuring the target.
2. **Zombie rider:** a deliberately powerful late-game pursuer. It hunts, shoots, closes and runs the victim down. Riders that meet can form persistent bands.
3. **Light:** a useful survival tool with believable consequences. Flashlights count; exposed elevated lights can attract attention from farther away; walls, curtains, terrain, weather and distance affect what actually escapes and who can perceive it.

The central playtesting questions are: **Did the stalker choose a horrible moment? Did the rider genuinely hunt me? Could I understand and manage the attention caused by my lights?**

## Settled intent and boundaries

- Daylight is **not an absolute prohibition** on stalker attacks.
- Sufficient nearby zombie pressure can outweigh its caution about exposure.
- Darkness creates another opportunity: an isolated person can be attacked without other zombies.
- Stalkers should operate over a couple of OMTs, using credible observations and remembered locations—not the player's unseen current coordinates.
- Riders should be aggressive individually and more dangerous together. Sharing a destination does not mean they have already met.
- Both creatures must participate beyond their locally spawned representation.
- Light attraction should share physical perception principles across hordes, bandits, cannibals, stalkers and riders. Their reactions remain different.
- Creature work remains separate from humanoid camps, dossiers, bounty, reporting and raids.
- Preserve existing worlds, actor identity and unrelated accepted work. Polish both creature descriptions.
- This brief does not authorise a phase-3 launch.

## 1. Light: make exposure and detection believable

### What needs changing

The current source scanner omits carried lights, approximates indoor escape with a three-tile exterior sightline, and combines sources within an OMT before fully separating their exposure. The observer path can then reject elevated sources. Hordes use a different, sound-style signalling route. These are interacting problems; adding a flashlight entry alone would leave inconsistent results.

Source anchors at the inspected baseline: `src/do_turn.cpp:6845` (`live_bandit_light_side_leakage_near`), `:6919` (`observe_live_bandit_field_signals_near_player`), `:7779` (`live_bandit_overmap_los_from`), `:7821` (`live_bandit_structural_observer_sight`), `:8635` (`live_bandit_staffed_camp_signal_reads`). The LOS helper rejects a different z-level outright. The observer's ordinary ambient-light sight also constrains the signal. Reverify symbols/lines before specifying.

### Desired behaviour

An exposed rooftop lamp, upper-storey window or hilltop fire should be observable from below. Raising a source should extend its practical visibility where it clears obstructions. Height must not make an enclosed upstairs room visible through solid walls or floors.

Distinguish:

- **Light emission:** whether the source is powered and producing light.
- **Escaping light:** what reaches a window, doorway or exterior surface.
- **Detection:** whether a particular observer can perceive that light.
- **Recognition:** whether it can identify a person or infer anything beyond "light over there."

A distant glow should not grant exact player position or camp knowledge.

### Proposed work

- Bring held, worn and weapon-mounted lights into the same physical-light path as ground lights and vehicle lamps.
- Respect power, dimming, containers and source movement.
- Resolve exposure per source before combining nearby signals.
- Replace the same-level restriction with meaningful vertical visibility and obstruction.
- Separate bright-source detection from ordinary dark-terrain recognition.
- Make source escape depend on actual geometry and attenuation. Large halls are not automatically exposed or automatically safe.
- Distinguish open windows, clear glass, curtains and shutters. Clear glass transmits light; opaque coverings provide concealment.
- Ensure short flashlight use can be noticed under suitable conditions, without every flick creating a long-lived tracking beacon.
- Support off-screen responses without scanning or generating the whole world. Turning off or leaving a source must stop refreshing it.

Research basis: NOAA distinguishes brightness/weather-limited visibility from height-limited geographic visibility. That supports treating height, source strength and obstruction separately, rather than adding an unconditional elevation bonus. Source: https://nauticalcharts.noaa.gov/publications/coast-pilot/files/cp5/CPB5_C01_WEB.pdf

CDDA already has cross-level visibility machinery that accounts for floors, and its lightmap handles transparency. Those are relevant foundations to investigate before inventing another incompatible visibility model. Source: https://github.com/CleverRaven/Cataclysm-DDA/blob/master/src/lightmap.cpp (`map::build_seen_cache`, `cast_zlight`, transparency/floor caches).

The rendering literature separates emitted light from obstruction and atmospheric attenuation. Borrow that distinction, not a full physical renderer. Source: https://www.pbr-book.org/4ed/Light_Sources/Light_Interface ; corresponding implementation https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/lights.h .

## 2. Writhing stalker: patience followed by commitment

### What needs changing

The current planner has useful cover and quiet-side behaviour, but visibility is treated as attention, bright exposure can veto a fight opportunity, and the attack budget advances during approach decisions. Its specialised off-screen stalking lifecycle is incomplete.

### Desired behaviour

A typical encounter should unfold like this:

**Notice → follow or search → recognise an opening → commit an approach → attack → break contact or continue according to danger.**

During a daytime city walk, it should linger and seek concealment. When zombies surround or converge on the player, it can exploit that pressure—even if the area is bright and the player can see it.

The current proposal interprets "enough zombies" as **perceptible, meaningful pressure**: zombies attacking or visibly closing on the same target. Zombies behind sealed walls, friendly zombies or an unrelated fight should not create imaginary opportunities.

### Proposed work

- Separate "visible to the target" from "being actively threatened."
- Let zombie pressure overcome daylight caution.
- Preserve a chosen approach long enough to reach actual contact.
- Count attack attempts at the attack stage, not while walking.
- Detect ineffective circling and resolve it into commitment, searching or genuine disengagement.
- Make "behind" mean the quieter side of the fight or a plausible interception route—not a fictional player-facing cone.
- Retain a credible retreat waypoint when driven away; prevent immediate reversal and reacquisition.
- Carry last-observed location, elapsed memory and identity across local/overmap transitions.
- Keep it weak. Improve timing before increasing damage or durability.

Existing-enemy basis: flesh raptors preserve a committed swoop rather than restarting their orbit every decision. Source: `src/monmove.cpp:582`, `apply_flesh_raptor_plan`.

Zombie hunters use a terrain-checked leap. If a literal pounce improves the feel, adapt that mechanism with appropriate opportunity gating; do not inherit the stronger relatives' entire behaviour. A leap is movement, not automatically a landed attack. Sources: `data/json/monsters/zed_misc.json:464`, `src/mattack_actors.cpp:162`, `leap_actor::call`.

## 3. Zombie rider: sustained pursuit and physical bands

### What needs changing

The present rider repeatedly skirmishes away, can withdraw when distant repositioning fails, and has no player-trampling attack. Its banding represents common light selection rather than encounters. The inspected saved-predator evolution route preserves empty ammunition, unlike direct spawning.

### Desired behaviour

**Investigate → acquire prey → pursue → shoot while closing → run down/contact → search the last credible location if contact breaks.**

The rider should remain oppressive through bow cooldowns and movement. It should not wait to exhaust every arrow before exploiting a close-range opportunity.

### Proposed work

- Establish natural-evolution/debug-spawn parity, including ammunition initialised once—not replenished by reloading.
- Repair pursuit beyond bow range and during cooldown.
- Replace routine post-shot flight with sustained pressure.
- Add a real physical run-down/contact attack, respecting terrain, occupancy and mount-sized passages.
- Remove routine half-health retreat as the default behaviour; reconsider it only if playtesting identifies a compelling reason.
- Form and merge rider bands through credible encounters. Preserve membership through separation, casualties and loading transitions.
- Share observed target evidence within a band without providing live coordinates of an unseen victim.
- Keep rider and mount composite initially; separate dismount identities are not required for this experience.

Existing-enemy basis: feral humans combine finite-ammunition ranged specials with ordinary pursuit and melee. The gun system does not inherently require retreat after firing. Source: `data/json/monsters/feral_humans.json:31`; rider post-shot flight is `HIT_AND_RUN` in `gun_actor::shoot` (`src/mattack_actors.cpp:1555`).

Existing melee actors support damage, knockdown and throwing. Those provide building blocks for impact, but copying a hulk's knockback would not itself produce cavalry behaviour—and could throw prey away from the pursuing rider. Sources: `data/json/monster_special_attacks/monster_attacks.json:983` (`hulk_wide_swing`), `:825` (`bio_op_takedown`), `:566` (`stag_smash`), `src/mattack_actors.cpp` (`melee_actor::call`, `on_damage`).

Dog/coyote social behaviour provides a perception-based grouping precedent, **not** a ready-made persistent rider squad. Blindly adding a swarm flag would be insufficient. Sources: `data/json/monsters/mammal.json` dog/coyote definitions; `src/monmove.cpp:1894` same-faction social scan and `rate_target` sight checks.

## Action plan and playtesting

| Work package | Discriminating playtests | Required result |
|---|---|---|
| Establish the baseline | Reproduce elevated-light rejection, stalker approach/burst issue and rider distant pursuit/evolution-ammo discrepancy | Record actual behaviour; distinguish source findings from observed failures |
| Light end to end | Same lamp at ground level, exposed above, behind a parapet and inside a sealed upper room; repeat with observer above/below | Cross-level detection works where exposed; height improves suitable sightlines; genuine obstruction still conceals |
| Flashlights and interiors | Held/worn/dropped; on/off/depleted; near window/deep hall; glass/curtains/door/corner; multiple differently exposed sources in one OMT | Local illumination and overmap consequences agree; no borrowed exposure or phantom source |
| Stalker commitment | Matched daytime encounters with no, modest and heavy zombie pressure; repeat in darkness; add hidden/unrelated zombies | Daylight assisted attacks occur, solitary-dark opportunities resolve, and false pressure does not trigger attacks |
| Rider pursuit and impact | Natural evolution; prey inside/outside bow range; cooldown, corner, doorway, vehicle, downed prey and empty bow | Actual pursuit, shots and contact; no retreat cliff, free ammunition, wall penetration or accidental permanent stun-lock |
| Off-screen continuity | Creatures initially abstract; moving light; target leaves; band encounter/separation; unload/reload and save/load | Same actors and intent continue without duplicates, teleport knowledge or simultaneous local/abstract movement |
| Integrated free-play | Ordinary daytime city looting, nighttime flashlight travel, lit-base exposure and late-game rider travel | The three intended experiences emerge without staging every decision |

For each package, use focused unit tests for rules, native multi-turn harness tests for consequences, and free-play for feel. Decision strings and successful setup are not substitutes for movement, attacks or attraction.

Measure detection delay, pursuit progress, actual attack attempts, lost-contact behaviour, turn cost and save growth against matched baselines. Let results establish sensible tuning; do not invent performance limits or zombie-count thresholds beforehand. Distinguish configurable, justified starting proposals from settled acceptance thresholds.

## Open choices to settle through prototypes

- Does the stalker feel better with a committed rush or a short physical leap?
- How much zombie pressure should overcome bright-light caution?
- Should rider "trampling" be a forceful contact attack, or a run-through manoeuvre that continues beyond the victim? Start with contact impact, then judge whether it delivers the intended experience.
- What flashlight exposure duration and distance produce understandable risk without making ordinary night travel unreasonably punishing?

## Handoff and evidence cautions

Target the existing Mac dev worktree, preserving its dirty work. Treat these as three explicit goals, not inherit the old creature exclusions.

Preserve the earlier light experiment as historical evidence of what code did. **Do not preserve cross-level rejection as desired behaviour.** Likewise, reconcile stale tests with this intent rather than letting old expectations dictate the design.

The retained light run `.userdata/dev-harness/harness_runs/20260907_152331_c3938384751f47dfa1e1049b7e3236b5` had isolated light source observations at game minutes 8580 and 8585, but eligible observers on another elevation returned `blocked_line_of_sight`; no light lead or approach was shown. This is historical source-positive/observer-rejection evidence, not successful light-only attraction or window/hall/free-play proof. A closure index also mislabeled a smoke callback artifact as light/optical evidence; verify actual channel fields when using old evidence.

The recommended first vertical slice is **an exposed elevated or carried light causing a legitimate off-screen observer response and physical approach**. Then develop stalker commitment and rider pursuit independently, before combining them in free-play passes.

Further code-audit risks to cover in the specification, not assume resolved: stalker cooldown names versus actual game-time advancement; fake pressure from unobserved zombies; identity and waypoint persistence; rider light response currently using local `g->all_monsters()` and positional pseudo-IDs; generic horde and creature-specific control competing over one entity; rider evolution gating at eight configured seasons (728 days with 91-day seasons, not fixed 730), catch-up through intermediate types, already-generated/old-save predators with explicit `upgrades:false`, and exactly-once ammunition initialization without reload refill. Distinguish existing source mechanics from proposed fixes and unproved live outcomes.


## Owner decision and delivery authorization — 2026-09-11

Josef explicitly chooses to skip backward-compatibility migration for old predators. Preserve `upgrades:false`; normal new-world evolution and save/load remain in scope. This supersedes any earlier unresolved legacy-predator migration choice. He authorizes implementation and playtesting of the frozen zombies/light FS, not unrelated project work, through the native de67 phase-3 supervisor with Sol low ordinary coordination and its workers.
