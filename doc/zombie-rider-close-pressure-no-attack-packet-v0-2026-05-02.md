# CAOL-ZOMBIE-RIDER-CLOSE-PRESSURE-NO-ATTACK-v0 — packet

Status: CLOSED / CHECKPOINTED GREEN V0 / BUGFIX + PRODUCT-FEEL FOLLOW-UP

Imagination source: `doc/zombie-rider-close-pressure-no-attack-imagination-source-2026-05-02.md`.

Checkpoint proof: `doc/zombie-rider-close-pressure-no-attack-proof-v0-2026-05-02.md`.

Live-watch notes: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/zombie-rider-live-watch-notes-2026-05-02.md`.

Seed run: `.userdata/dev-harness/harness_runs/20260502_015857/` from `zombie_rider.live_open_field_pressure_mcw` handoff. Caveat: this handoff is yellow/inconclusive because of runtime-version mismatch, so use it as a bug seed and player-observed taste note, not as closure proof.

## Summary

Josef quit the live watch and greenlit this package after observing that the zombie rider was in/near the house with him and was not attacking at all. The important smell is not merely “needs more aggression”: the trace reportedly repeats `decision=bow_pressure`, `reason=line_of_fire`, `line_of_fire=yes`, distance around `4-5`, while the player sees no visible attack. Andi must name and fix the missing bridge between rider decision and visible action before tuning broader aggression.

The intended product feel is a scary mounted archer that shoots when it can and irregularly bunny-hops/circles/repositions around the player group when blocked or too close. Not a perfect geometric orbit; not instant unavoidable death; not polite indoor loitering.

## Scope

- Reproduce or minimize the no-attack condition from the 2026-05-02 watch: rider close/in-house or near-house, player and possible allies nearby, expected visible mounted pressure.
- Inspect the actual bridge from `zombie_rider live_plan: decision=bow_pressure reason=line_of_fire` to the action layer. Determine whether the missing attack comes from ammo/weapon state, special attack dispatch, cooldown/cadence, line-of-fire/action mismatch, targeting/faction hostility, safe-mode/handoff pause effects, runtime mismatch, or another concrete cause.
- Fix the smallest honest seam so `bow_pressure + line_of_fire=yes` produces visible attack pressure or a named and tested refusal path.
- Add close/indoor pressure scoring so the rider shoots when possible and otherwise repositions with irregular bunny-hop/circling pressure around the player group where terrain allows.
- Include the zombie rider description polish as part of the same visible-fantasy packet. Candidate text:

  > Up on a six-legged horse — or is it a spider? — a towering figure with eyes the color of blood holds a gory bow of wet bones and sinews.
  > It moves ferociously, tumbling feet hastening across the terrain.
  > Running is out of the question.

## Non-goals

- Do not reopen the whole `CAOL-ZOMBIE-RIDER-0.3-v0` lane.
- Do not merely crank aggression numbers without naming the no-attack cause.
- Do not break wounded disengagement, cover/LOS counterplay, no-light controls, or camp-light band/circle-harass footing.
- Do not turn bunny-hop pressure into perfect geometric orbiting.
- Do not make the rider an unavoidable instant-kill machine.
- Do not interrupt the active camp-locker cleanup lane unless Schani/Josef explicitly promotes this item over it.

## Success state

- [x] A focused repro or minimized test/harness scenario demonstrates the original no-attack smell or explains why the yellow seed run cannot be trusted.
- [x] The root cause of `bow_pressure + line_of_fire=yes` producing no visible attack is named with source evidence.
- [x] A focused fix makes close/indoor rider pressure visibly hostile: shoot when line of fire and action resources allow; otherwise reposition/bunny-hop/circle irregularly instead of idling.
- [x] Deterministic tests cover the decision-to-action bridge and at least one close/indoor reposition/pressure case.
- [x] Fresh live/handoff validation shows a close rider attacking or aggressively repositioning within a few turns, with screenshots/artifacts and no runtime-version mismatch if claiming feature proof.
- [x] Existing rider guarantees still pass: wounded disengagement, blocked LOS/cover counterplay, overmap light attraction/band cap, no-light negative control, and no wall-suicide/perfect-orbit behavior.
- [x] Zombie rider description text is updated/preserved according to the corrected product image.

## Testing and evidence expectations

Minimum gates:
- `git diff --check`.
- Focused zombie rider unit tests covering the bridge and close-pressure behavior.
- Existing `[zombie_rider]` suite or the narrow equivalent if build cost is high.
- Fresh tiles build before live/handoff proof if screenshots or feature proof are claimed.
- Fresh live/handoff row with clean startup/version gate before claiming this fixed in-game behavior.

Evidence must separate:
- deterministic decision/action proof;
- live on-screen behavior;
- artifact/log paths;
- caveats from the yellow 2026-05-02 watch seed.

## Known seed artifacts

- Seed run: `.userdata/dev-harness/harness_runs/20260502_015857/`.
- Runtime watch note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/zombie-rider-live-watch-notes-2026-05-02.md`.
- Screenshot attempts: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/rider-in-house-not-attacking-live-note.png`, `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/rider-not-attacking-at-all-live-note.png`.
