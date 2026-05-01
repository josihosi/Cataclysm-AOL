# CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

Status: ACTIVE / GREENLIT / PREDATOR VARIETY PACKAGE / OLD-FEELING COMPARISON ROW GREEN / REVIEW NEXT

Imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

## Summary

Retrofit flesh raptors from plain `HIT_AND_RUN` annoyance into visible circling skirmishers. They may still swoop, bite, and withdraw, but their primary readable pattern should be orbiting at roughly 4–6 tiles, moving toward under-defended arcs, avoiding the main zombie blob, and committing periodically instead of acting like a one-note melee skill check with equipment damage attached.

## Scope

- Audit current flesh raptor IDs and descendants (`mon_spawn_raptor`, shadow/unstable/electric/fungal variants) and their current `HIT_AND_RUN` / special-attack behavior.
- Introduce the smallest reusable circling/orbit scorer needed for visible skirmisher behavior, preferably without changing every `HIT_AND_RUN` monster globally.
- Score candidate tiles for 4–6 tile spacing, lateral/orbit movement around the player, open flanking arcs, low crowding by other enemies, and terrain passability.
- Add hysteresis/cadence so raptors hold an orbit intention for a few turns instead of jittering every turn.
- Preserve a fallback for corridors, blocked terrain, and direct adjacency where circling is impossible.
- Review equipment-destruction frustration as part of the package; do not increase it, and reduce/soften it if the current behavior remains anti-fun after movement changes.

## Non-goals

- Do not remove every attack-and-retreat enemy from the game.
- Do not redesign eigenspectres or other portal-storm enemies in this package.
- Do not make raptors a full tactical-pack AI with memory, communication, or overmap behavior.
- Do not globally rewrite `HIT_AND_RUN` unless the narrow raptor seam proves impossible.
- Do not tune difficulty only by raising damage, armor piercing, or equipment destruction.

## Success state

- [x] Flesh raptor behavior no longer depends solely on generic `HIT_AND_RUN` for its main readable pattern when open circling terrain exists.
- [x] Deterministic map tests prove a raptor prefers a valid 4–6 tile orbit/flank position over straight retreat when open lateral space exists.
- [x] Deterministic crowding tests prove a raptor prefers the under-occupied arc rather than stacking into the same zombie/enemy side.
- [x] Corridor/blocked-terrain tests prove graceful fallback without jitter loops or stuck non-actions.
- [x] Live/playtest rows compare old-feeling stab/flee frustration against new circling pressure with metrics for orbit turns, swoop cadence, hit count, equipment-damage events, player counterplay, and warnings/errors.
- [ ] Review whether the agent-side old-feeling comparison is enough for perceived fun/annoyance closure, or whether Josef should get a taste/playtest caveat before final close.
- [x] Existing JSON/load and focused monster tests remain green for raptor variants touched by the package.

## Evidence checkpoint

- Deterministic/test/build gate: focused `tests/flesh_raptor_test.cpp` rows are green, full tiles build is green, and `git diff --check` is clean; `astyle-diff` remains unavailable because `astyle` is not installed on this host.
- Staged live feature rows: open field (`.userdata/dev-harness/harness_runs/20260501_052709/`), crowded arc (`.userdata/dev-harness/harness_runs/20260501_053414/`), and blocked corridor (`.userdata/dev-harness/harness_runs/20260501_054807/`) are credited feature-path proof for orbit/swoop/fallback behavior.
- Promoted old-feeling/equipment comparison row: `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/` is green feature-path proof (`7/7` step ledger, `artifacts_matched`, `feature_proof True`). It combines current mechanics evidence (`decision=orbit`, `decision=swoop`, `flesh_raptor melee_event: ... target_hp_percent=... run_after=yes`) with screenshot/OCR player-facing frustration evidence from message history (`flesh-raptor`, `impales`, `cut!`, `You put pressure on the bleeding wound...`).
- Caveat: these are staged-but-live McWilliams rows, not natural random-discovery/release packaging proof. Equipment-damage tuning was not changed; the row keeps equipment damage visible as a frustration metric rather than solving taste by making the raptor hit harder or shred more gear.

## Targeted tests

- Unit/helper tests for orbit candidate scoring: ideal distance 4–6, lateral movement, under-occupied arc, passability, and crowding penalty.
- Deterministic small-map tests:
  - open-space raptor circles instead of straight retreat;
  - raptor avoids zombie-occupied/front pressure side;
  - blocked corridor falls back to ordinary approach/retreat without jitter;
  - cadence/hysteresis prevents target-tile thrashing.
- Live/playtest scenarios:
  - one raptor in open street;
  - several raptors around a normal zombie cluster;
  - corridor/interior negative-control;
  - equipment-damage/frustration audit.

## Fun metrics

- Orbit turns before commit.
- Average/median player distance while skirmishing.
- Swoop attempts, successful hits, misses, and withdrawal/reposition turns.
- Number of target-tile changes per 20 turns as a jitter smell.
- Equipment damage/destruction events.
- Player counterplay success: cornering, using cover, using corridor, using ranged/fire/light/noise where relevant.
- Turn-time cost and log/debug noise.

## Cautions

This should make raptors less anti-fun, not more sophisticatedly obnoxious. If the new behavior only makes them harder to hit while preserving equipment shredding, the package failed with nicer choreography.
