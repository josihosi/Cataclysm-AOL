# Andi handoff: Bandit structural bounty Phase 4

## Current canon state

`Bandit structural bounty overmap completion packet v0` is the **ACTIVE / GREENLIT IMPLEMENTATION PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, and `TESTING.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

Phase 3 deterministic structural outing planner/resolver is locally green. Checkpoint commit: `b8fc4b1d8c Add structural bounty outing resolver` on `dev`.

Credited Phase 3 claims:

- small non-player structural outings can choose forest/town `structural_bounty` camp-map leads without using the player-pursuit handoff as terrain dispatch proof;
- forest/town choice is deterministic from bounty + confidence - known threat;
- active outside pressure blocks parallel same-site structural dispatch/dogpiling;
- stale plans are revalidated against active pressure, reserve, at-home readiness, lead status, recent-check debounce, and known low interest before apply;
- stalking-distance threat reveal happens before arrival and does not consume bounty by itself;
- revealed threat subtracts from interest and can turn the outing back before arrival;
- arrival consumes structural bounty only after interest survives, then writes harvested/debounce state;
- member/outbound state and active structural group fields clear on return;
- recently checked/low-interest structural targets are not immediately reselected.

Evidence credited for Phase 3: `git diff --check`; `make -j4 obj/bandit_live_world.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world][structural_bounty]" --success` -> 216 assertions in 18 test cases. No live game claim is credited yet.

## Active work

Current state boundary: **Phase 4 save/load and anti-loop**.

Implementation scope for the next run:

1. Add or extend serialization coverage for structural bounty leads, active structural outings, harvested/dangerous outcomes, and member active/returned state.
2. Prove arrival after reload consumes structural bounty once, not twice, and preserves the stalking-before-arrival distinction.
3. Add deterministic 500-turn playback for forest/town progression: no repeated harvested tile, no ping-pong against dangerous/low-interest tiles, and no stuck active outing.
4. Add or verify counters for dispatches planned, stalking checks, lost-interest returns, arrivals/harvests, skipped harvested/dangerous/recently-checked leads, and bounded multi-camp work.
5. Run a short Ralph Wiggum pass: save/load must not erase risk memory, harvested/dangerous debounce must survive long enough to stop repeat loops, and counters must prove bounded work rather than only final status.

## Success bar for Phase 4

Phase 4 is green only when deterministic evidence proves:

- save/load preserves structural leads, active structural outings, harvested/dangerous outcomes, and member active/returned state;
- arrival after reload consumes structural bounty once, not twice;
- harvested tiles do not regenerate from later structural scan;
- 500-turn playback covers forest/town progression, danger/low-interest debounce, harvest debounce, and no stuck active outing;
- multi-camp scan/outing work stays bounded by counters;
- the Ralph Wiggum smell pass says the ecology avoids repeat magnets and hidden unbounded work.

## Testing/evidence expectations

Use the smallest honest evidence for the slice.

Suggested narrow gates after Phase 4 code changes:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][structural_bounty]" --success`

Broaden only if serialization/playback touched files justify it. Do not run live/harness proof yet; the promoted live target waits until deterministic scan/outing/save-load substrate is green and wired into the real maintenance path.

## Non-goals/cautions

- Do **not** start live/harness proof in Phase 4.
- Do **not** implement writhing stalker, local bubble materialization, exact loot harvest, player/Josef-facing tuning, release packaging, or random casualty economy beyond deterministic counters.
- Do **not** promote old Smart Zone, source-zone fire, bandit signal, roof-fire horde, watchlist, or release work from this handoff.
- Do **not** rerun solved proof packets as ritual.
- Do **not** let this handoff overrule `Plan.md` / `TODO.md` / `SUCCESS.md` / `TESTING.md`.

## Canonical anchors

- Active roadmap: `Plan.md`
- Active queue: `TODO.md`
- Success ledger: `SUCCESS.md`
- Testing ledger: `TESTING.md`
- Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`
- Imagination source: `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`
- Testing ladder: `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`

## Superseded handoff note

This file supersedes the old Smart Zone live coordinate-label handoff. Smart Zone Manager live coordinate-label proof remains closed/green in `TESTING.md` snapshots and its aux docs, but it is not the active execution target.
