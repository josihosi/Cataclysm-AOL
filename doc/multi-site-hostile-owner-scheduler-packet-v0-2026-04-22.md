# Multi-site hostile owner scheduler packet v0 (2026-04-22)

Status: checkpointed / done for now.

## Why this exists

The active `Bandit live-world control + playtest restage packet v0` lane is finally making one live hostile owner real inside the game.
That is necessary, but it is still only one-site proof.

The next hollow seam is obvious:
if the repo can only run one honest hostile owner at a time, then later cannibal / feral / other hostile-site work will either fake independence or quietly collapse back into one spooky shared bandit brain.

This packet exists to prove the hostile-owner substrate can run a **small set of independent hostile sites** at once and keep their state separate.
That means separate anchors, rosters, outings, remembered pressure, and writeback.
Not one giant coalition blob with better marketing.

## Scope

1. Extend the current live hostile-owner seam from one-site-at-a-time proof to `2-3` simultaneous hostile sites on one world.
2. Keep each hostile site's state independent and explicit:
   - anchor / home site
   - roster and live remaining headcount
   - active outing or local-contact state
   - remembered pressure / marks / target state
   - persisted writeback strong enough to survive save/load honestly
3. Preserve the current product rule that site-backed camps keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts.
4. Reuse existing territoriality / active-pressure / distance-burden footing to damp silly same-target pile-ons rather than inventing magical coalition coordination.
5. Keep the packet small enough that review can still answer the question `do these hostile sites behave like separate owners?` without needing a grand-strategy lecture.

## Non-goals

- faction grand strategy or coalition command behavior
- dozens of hostile families at once
- social-threat, toll, or extortion implementation
- giant whole-world omniscient hostile maps
- broad new local tactical AI
- profile-layer generalization beyond what is needed to keep several owners independent

## Success shape

This packet is good enough when:
1. the live hostile-owner seam can run `2-3` simultaneous hostile sites on one world without collapsing them into one shared fake camp brain
2. each hostile site keeps its own anchor/home site, roster/headcount, active outing/contact state, remembered pressure/marks, and persisted writeback state
3. site-backed camps still keep a home presence while losses continuously shrink later outings instead of snapping back to folklore counts
4. one site's losses, return state, or contact pressure do not silently rewrite another site's state
5. repeated same-target pressure can overlap occasionally without turning into routine multi-site dogpile coalition behavior
6. save/load stays honest for multiple hostile owners at once instead of only for the single easiest happy-path site
7. the slice stays bounded and reviewer-readable instead of widening into hostile-human empire

## Validation expectations

- prefer narrow deterministic coverage for multi-owner save/load, state separation, and independent writeback before broader live drama
- add at least one controlled proof where multiple hostile sites coexist and later divergence is visible reviewer-cleanly
- if overlap pressure is demonstrated, make the output readable enough to show `independent convergence happened` rather than `the sites secretly merged`
- broaden to live probe only when deterministic or tightly controlled proof stops being honest enough to answer the independence question

## Landed evidence

- Deterministic separation proof lives in `tests/bandit_live_world_test.cpp`: `bandit live world keeps several hostile sites independent across save and writeback` builds three claimed hostile sites, assigns independent active outings/marks/pressure, round-trips the state through JSON, applies one site's loss/writeback, and checks the other sites stay untouched.
- The live scheduler seam now permits bounded overlapping player pressure instead of returning after the first valid site. `steer_live_bandit_dispatch_toward_player()` counts existing player-targeted active pressure and caps simultaneous player-pressure outings at `2`, preserving occasional overlap without inventing a coalition brain.
- Current-build live proof lives at `.userdata/dev-harness/harness_runs/20260424_003005/`: scenario `tmp.bandit_multi_site_two_site_dispatch_probe_1860` starts from two claimed nearby hostile owners, disables safe mode, advances `1860` turns across the `30_minutes` scheduler cadence, save-quits cleanly, and copies the saved world for inspection.
- Copied-save inspection shows two separate active records targeting `player@140,43,0`: `overmap_special:bandit_camp@140,51,0` keeps `headcount = 14`, member `4` `state = outbound`, `active_group_id = overmap_special:bandit_camp@140,51,0#dispatch`, and its own remembered mark/pressure; `overmap_special:bandit_camp@140,44,0` keeps `headcount = 7`, member `18` `state = local_contact`, `active_group_id = overmap_special:bandit_camp@140,44,0#dispatch`, and its own remembered mark/pressure.
- Final narrow validation passed via `make -j4 TILES=1 SOUND=1 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/do_turn.o cataclysm-tiles`, `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, `python3 tools/openclaw_harness/startup_harness.py list-scenarios`, `git diff --check`, and direct inspection of `.userdata/dev-harness/harness_runs/20260424_003005/saved_world/McWilliams`.

## Dependency note

This packet landed behind `doc/bandit-live-world-control-playtest-restage-packet-v0-2026-04-22.md` after that lane had honest nearby restage/writeback/perf proof.
The next lane is `doc/hostile-site-profile-layer-packet-v0-2026-04-22.md`; do not blur that profile-layer work back into this completed bandit-only multi-site proof.
