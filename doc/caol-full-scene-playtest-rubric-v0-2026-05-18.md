# CAOL full-scene playtest rubric v0

Use this for the `theme.*` OpenClaw scenarios. These are not narrow proof rows; they are reviewer-facing scene tests for whether CAOL systems compose into good play.

## Required readout

For each run, record:

- Scenario name, run directory, runtime title/version, fixture name, and whether startup was current or stale.
- Screenshots/artifacts used: before pressure, first visible contact, branch choice, mid-scene, resolution or handoff stop, and post-save/reload when applicable.
- Player outcome: escaped, paid, won, lost, stalemate, or inconclusive.
- One actor row each for player, camp NPCs, bandits, cannibals, special monsters, hordes, and camp systems present in the scene.

## Actor row

Use this compact row format:

| Actor group | Intended role | What they actually did | Good behavior | Weird behavior | Verdict |
| --- | --- | --- | --- | --- | --- |
| camp NPCs | defend intact camp |  |  |  |  |

Verdict must be one of:

- `fun/intense`
- `dynamic but rough`
- `mechanically correct but dull`
- `idiotic behavior`
- `bug/regression`
- `inconclusive`

## Judgment questions

- Did the scene create readable pressure without hiding decisive state in logs only?
- Did allies and hostiles pick sensible targets and move through normal pathing?
- Did shakedown, camp defense, patrol, locker, and trade/payment surfaces stay separate instead of leaking into each other?
- Did bandits, cannibals, stalkers, riders, raptors, and hordes keep distinct behavior identities?
- Did reload/writeback preserve believable aftermath state without duplicate pressure, frozen actors, or impossible hostility?

## Evidence rules

- Do not call a scene fun because a unit tag passed. Use screenshots, message text, debug artifacts, and saved-state metadata together.
- Do not call a scene broken because it is hard. Mark it broken only when behavior is incoherent, unfair, inert, duplicated, or clearly contradicts the feature contract.
- If automation only reaches startup/load, classify the run as `load proof only / inconclusive`.
- If a scenario is intentionally dense, separate actor rows before giving the whole-scene verdict.

## Current theme scenarios

- `theme.intact_camp_shakedown_fight_mcw`: main question is whether choosing Fight inside an intact armed camp creates a believable camp-defense fight.
- `theme.mixed_hostile_camp_siege_mcw`: main question is whether stacked CAOL hostile primitives produce layered tension or unreadable noise.
- `theme.aftermath_reload_cleanup_mcw`: main question is whether a saved aftermath reloads, advances, and writes back without duplication or stale pressure.

## Feature coverage matrix

Use the theme scenarios as cross-feature checks. A feature is not credited from one green metadata gate; it needs two or three scene observations from this matrix.

| Feature area | Playtest checks |
| --- | --- |
| Bandit shakedown Pay/Fight surface | `theme.intact_camp_shakedown_fight_mcw` must show visible Pay/Fight choice; Fight branch must log demanded/reachable values; post-Fight turns must show real consequences rather than a silent close. |
| Intact camp defense with armed NPCs | Preflight NPC snapshot must show armed nearby camp NPCs; fight window must record whether allies target/move sensibly; aftermath probe must check survivors or duplicates after save/reload. |
| Bandit live-world active pressure | Intact-camp shakedown must reach local contact; mixed siege must preflight an active bandit stalk job; mixed siege overmap wait must emit compact active-job/perf cadence. |
| Cannibal hostile-site pressure | Mixed siege must preflight a cloned cannibal-style hostile site; local window must judge whether cannibal pressure reads differently from bandits; overmap cadence must keep the cannibal stalk row separate. |
| Writhing stalker | Mixed siege must preflight a saved stalker; local window must judge target choice, shadow/strike readability, and interference with bandits/NPCs. |
| Zombie rider and flesh-raptor | Mixed siege must preflight both staged monsters; local window must judge whether each creates recognizable pressure; actor rows must separate rider/raptor weirdness from bandit behavior. |
| Horde plus fire/smoke signal | Mixed siege must preflight the horde and live fire/smoke cue; local window must note whether noise/light pressure is readable; overmap wait must not collapse into stale or duplicated pressure. |
| Save/reload/writeback cleanup | After any handoff save, run `theme.aftermath_reload_cleanup_mcw`; it must prove time passage, save mtime, saved turn delta, NPC snapshot, and bandit-live-world snapshot. |
