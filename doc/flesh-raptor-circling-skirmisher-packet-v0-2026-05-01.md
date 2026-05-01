# CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

Status: GREENLIT / BACKLOG / PREDATOR VARIETY PACKAGE

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

- [ ] Flesh raptor behavior no longer depends solely on generic `HIT_AND_RUN` for its main readable pattern when open circling terrain exists.
- [ ] Deterministic map tests prove a raptor prefers a valid 4–6 tile orbit/flank position over straight retreat when open lateral space exists.
- [ ] Deterministic crowding tests prove a raptor prefers the under-occupied arc rather than stacking into the same zombie/enemy side.
- [ ] Corridor/blocked-terrain tests prove graceful fallback without jitter loops or stuck non-actions.
- [ ] Live/playtest rows compare old-feeling stab/flee frustration against new circling pressure with metrics for orbit turns, swoop cadence, hit count, equipment-damage events, player counterplay, warnings/errors, and perceived fun/annoyance.
- [ ] Existing JSON/load and focused monster tests remain green for raptor variants touched by the package.

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
