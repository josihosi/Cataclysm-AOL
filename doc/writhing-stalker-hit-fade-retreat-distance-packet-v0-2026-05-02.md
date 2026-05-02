# CAOL-WRITHING-STALKER-HIT-FADE-RETREAT-DISTANCE-v0 — packet

Status: GREENLIT / QUEUED / PRODUCT-TUNING + BEHAVIOR FIX FOLLOW-UP / AFTER ACTIVE CAMP-LOCKER LANE UNLESS PROMOTED

Imagination source: `doc/writhing-stalker-hit-fade-retreat-distance-imagination-source-2026-05-02.md`.

Runtime watch notes: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.

Seed run: `.userdata/dev-harness/harness_runs/20260502_015032/` from `writhing_stalker.live_alley_predator_mcw` handoff. Caveat: this handoff was yellow/inconclusive due to runtime-version mismatch; use it as a player-observed seed, not closure proof.

## Summary

Josef greenlit a focused follow-up after live-watching the writhing stalker. Desired rhythm: attack about 2-4 times depending on stress, then retreat farther — probably at least about 8 tiles — more like the original flesh raptors, not the changed circling raptors. Earlier watch notes also flagged that light plus two friendly NPCs should push the stalker toward retreat/caution.

This is not a whole stalker redesign. It is a bounded tuning/behavior package for burst length, retreat distance, stress/counterpressure, and old-raptor reference checking.

## Scope

- Check original/pre-circling flesh raptor hit-and-run behavior as the reference feel. Do not rely on memory; inspect old code, old tests, commits, or a controlled current comparison if the original seam is still recoverable.
- Reproduce or minimize current stalker post-attack behavior from a live or deterministic setup: burst length, retreat distance, and whether it A/B oscillates instead of disengaging.
- Add or adjust stalker logic so post-burst retreat is meaningful: target around 8+ tiles when space/pathing allows, with graceful fallback when terrain blocks it.
- Stress/counterpressure should shape persistence: light, focused player, multiple allied NPCs, injury, or exposed terrain should reduce attack count and increase retreat caution; darkness, distraction, zombie pressure, and target vulnerability may allow 2-4 attacks before retreat.
- Preserve closed stalker guarantees: no omniscience, no constant strike spam, injured retreat, light/focus counterplay, and zombie-shadow/quiet-side behavior.

## Non-goals

- Do not reopen all `CAOL-WRITHING-STALKER-v0` or all live fun scenarios.
- Do not copy the new flesh-raptor circling/orbit behavior into the stalker.
- Do not make the stalker always attack 3-4 times regardless of light/allies/stress.
- Do not make it harmless by always fleeing at first sight.
- Do not interrupt the active camp-locker cleanup lane unless Schani/Josef explicitly promotes this item over it.

## Success state

- [ ] Original flesh-raptor hit-and-run behavior is checked and summarized as the reference baseline, including likely retreat distance/rhythm where evidence allows.
- [ ] Current stalker burst/retreat behavior is reproduced or minimized, with retreat distance and decision reasons visible.
- [ ] Stalker post-burst retreat aims for about 8+ tiles when pathing allows, with tested fallback when blocked.
- [ ] Stress/counterpressure affects burst length and retreat: light plus multiple allies causes earlier/farther caution; high-stress dark/distraction cases may allow 2-4 attacks before disengage.
- [ ] Deterministic tests cover retreat distance, stress-modulated burst count/caution, and no A/B near-player oscillation after the burst.
- [ ] Fresh live/handoff validation shows readable attack burst then real disengage, with screenshots/artifacts and clean startup/version gate if claiming feature proof.
- [ ] Existing stalker guarantees still pass: no omniscience, no constant strike spam, injured retreat, light/focus counterplay, and zombie-shadow/quiet-side behavior.

## Testing and evidence expectations

Minimum gates:
- `git diff --check`.
- Focused writhing stalker unit tests for burst/retreat distance and stress/counterpressure.
- Existing `[writhing_stalker]` suite or narrow equivalent covering previous guarantees.
- Reference note for original flesh raptor hit-and-run evidence.
- Fresh live/handoff row with clean startup/version gate before claiming in-game feature proof.

Evidence must separate:
- original raptor reference evidence;
- deterministic stalker behavior proof;
- live on-screen behavior;
- artifact/log paths;
- caveats from the yellow 2026-05-02 watch seed.

## Known seed artifacts

- Seed run: `.userdata/dev-harness/harness_runs/20260502_015032/`.
- Runtime note: `/Users/josefhorvath/.openclaw/workspace/runtime/caol-live-watch-20260502/writhing-stalker-live-watch-notes-2026-05-02.md`.
- Reported watch observations: initial A/B pacing before eventual attack; no retreat under light plus two friendly NPCs; desired 2-4 stress-shaped attacks then about 8+ tile retreat.
