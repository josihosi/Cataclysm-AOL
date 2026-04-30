# Andi handoff: Bandit structural bounty Phase 6

## Current canon state

`Bandit structural bounty overmap completion packet v0` is the **ACTIVE / GREENLIT IMPLEMENTATION PACKET**.

Authoritative canon is `Plan.md`, with `TODO.md`, `SUCCESS.md`, and `TESTING.md` aligned downstream. This handoff is only a terse executor packet; if it ever disagrees with those files, repair this file from canon instead of treating it as truth.

Phase 4 save/load and anti-loop is locally green. Checkpoint commit: `ee0d8129b0 Add structural bounty save-load anti-loop tests` on `dev`.

Phase 5 live wiring is locally green in the current checkpoint: `do_turn` maintenance now invokes bounded structural bounty maintenance on a 60-minute cadence, uses per-candidate overmap terrain lookup instead of a global sweep, emits concise structural maintenance reports, and the current tiles runtime builds. No live game claim is credited yet.

## Current state boundary

Current state boundary: **Phase 6 live proof**.

Next executor slice:

1. Shape the narrow harness scenario for idle camp -> structural forest/town dispatch without player smoke/light/direct-range bait.
2. Include saved-state metadata audits for structural leads, active/returned members, active outing fields, stalking-distance threat/interest writeback, harvested/dangerous outcome, and debounce/no immediate repeat.
3. Validate any new scenario JSON with `python3 -m json.tool` and run `python3 tools/openclaw_harness/proof_classification_unit_test.py` if classifier/report logic changes.
4. Run one feature-path harness probe only after the scenario has explicit visible/artifact expectations; do not treat startup/load as feature proof.
5. Update canon with the exact run path, saved-state evidence, and any live-path caveat before moving to Phase 7 tuning/readout.

## Credited evidence

Phase 4 credited evidence:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests/bandit_playback_test.o LINTJSON=0 ASTYLE=0`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][structural_bounty]" --success` -> 382 assertions in 22 test cases
- `./tests/cata_test "[bandit][playback][structural_bounty]" --success` -> 88 assertions in 2 test cases

Phase 5 credited evidence:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests/bandit_playback_test.o LINTJSON=0 ASTYLE=0`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][structural_bounty]" --success` -> 411 assertions in 23 test cases
- `./tests/cata_test "[bandit][playback][structural_bounty]" --success` -> 88 assertions in 2 test cases
- `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`

## Phase 5 behavior now in place

- `overmap_npc_move()` bootstraps live bandit sites for structural maintenance and calls `maintain_live_bandit_structural_bounty()` on `calendar::once_every( 60_minutes )`.
- `maintain_live_bandit_structural_bounty()` calls `advance_structural_bounty_maintenance()` with `structural_scan_budget = 12` and `structural_dispatch_cap = 2`.
- The maintenance helper advances active structural outings, scans bounded candidates, plans/applies capped structural dispatches, and renders one concise report covering scan budget/use, sampled/seeded/suppressed leads, dispatches, active outings, stalking checks, turnbacks, arrivals, and returned members.
- The deterministic maintenance test proves the combined seam can seed a structural lead, dispatch a member, process stalking contact, skip active-outside scan churn, process arrival/harvest, return the member, and render report counters.

## Non-goals/cautions

- Do **not** implement writhing stalker, local bubble materialization, exact loot harvest, player/Josef-facing tuning, release packaging, or random casualty economy beyond deterministic counters.
- Do **not** promote old Smart Zone, source-zone fire, bandit signal, roof-fire horde, watchlist, or release work from this handoff.
- Do **not** rerun solved Phase 4/Phase 5 proof packets as ritual.
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

This file supersedes the old Phase 4 save/load handoff, the stale Phase 5-start handoff, and the older Smart Zone live coordinate-label handoff. Phase 4 and Phase 5 are closed locally; Smart Zone Manager live coordinate-label proof remains closed/green in `TESTING.md` snapshots and its aux docs, but none of those are the active execution boundary.
