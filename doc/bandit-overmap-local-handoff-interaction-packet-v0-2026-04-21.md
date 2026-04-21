# Bandit overmap/local handoff interaction packet v0 (2026-04-21)

Status: active contract.

## Greenlight verdict

Josef explicitly wants the next promotion to push the current bandit overmap work toward a real playtesting-ready point instead of drifting into endless clarification notes.
The immediate gap is not raw signal generation anymore.
It is the handoff behavior between abstract overmap intent and local play.

This packet is the active next slice because it carries the main anti-weirdness burden:
- overmap intent must become local posture, not magic tile puppetry
- local danger/contact should be able to rewrite the abstract plan instead of serving it forever
- the result should be readable enough that later playtesting notes mostly become tuning or taste notes, not basic "what is this system even doing" confusion

Together with the queued elevated-light / z-level packet behind it, this is the current playtesting-readiness train.

## Scope

1. Land one bounded handoff interaction packet on the current bandit playback / proof seam, focused on overmap-to-local and local-to-overmap behavior rather than new signal sources.
2. Prove that overmap intent hands off as coarse local posture, for example investigate, stalk, hold, probe, or retreat, and **not** as exact-square omniscience or stale route puppetry.
3. Prove that once local engagement or obvious local danger is active, local reality can override the prior overmap posture instead of being dragged around by it.
4. Prove sane contact-loss and reacquisition behavior on the current seam: loss of contact should degrade to rough search, cooldown, or disengage instead of immortal exact tracking.
5. Keep stalking behavior bounded and readable: ordinary cautious pressure should preserve stand-off behavior instead of crowding the immediately adjacent overmap tile or feeling like house-next-door psychic shadowing.
6. Include a small named scenario family that covers at least:
   - posture-not-tile-puppetry on entry
   - local-engagement override or retreat rewrite
   - contact loss / reacquisition without omniscience
   - return-state carryback after the local scene changes the group
   - at least one player-present context such as basecamp pressure, follower travel, or vehicle / convoy interception, so the seam is not only tested against a static toy target
7. Keep the return path honest: wounds, panic, losses, cargo change, threat change, and target-confidence change should be able to flow back into the abstract group state reviewer-cleanly enough to judge whether the seam is real.
8. Extend deterministic coverage so the named packet and its rendered output are proven directly on the current tree.

## Non-goals

- a full local combat AI rewrite
- coalition strategy, campaign planning, or deliberate multi-camp coordination
- a broad new visibility or concealment system
- the elevated-light / z-level packet that sits behind this one
- tactical bandit-versus-zombie combat simulation
- a general NPC persistence architecture rewrite
- free raid/extraction truth or exact-square remote omniscience

## Success shape

This item is good enough when:
1. one bounded overmap/local handoff interaction packet exists on the current proof seam
2. deterministic proof shows overmap intent enters local play as posture rather than exact-square puppetry
3. deterministic proof shows local engagement, danger, or loss of contact can honestly rewrite or cool the prior overmap posture instead of preserving stale certainty forever
4. at least one player-present context beyond a static toy camp is covered, so the seam is not only "fine" in the easiest synthetic case
5. reviewer-readable output shows the entry posture, any local rewrite, and the returned abstract-state change clearly enough to debug by reading the packet instead of guessing from side effects
6. the slice stays bounded and does not widen into full combat AI, broad perception rewrites, or a general architecture sermon

## Validation expectations

- prefer real multi-turn proof on the current overmap-side playback seam, up to `500` turns where needed
- keep per-scenario goals and tuning metrics explicit instead of vague vibes
- deterministic coverage should check the rendered packet or explicit state fields directly where possible, not only hidden internals
- the important reviewer question is whether the seam now reads like a coherent world interaction rather than a haunted routing trick

## Product guardrail

The player should read this as a bandit group entering local play with an intention, then reacting like the world actually happened to them.
Not like a spreadsheet reached through the ceiling and grabbed a tile.

If later Josef playtesting still mainly produces "why are they psychic" or "why did they snap to that square" notes after this packet lands, the packet did not really clear the intended bar.
