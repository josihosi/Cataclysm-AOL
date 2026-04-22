# Bandit + Basecamp playtest kit packet v1 (2026-04-22)

Status: GREENLIT / QUEUED AFTER V0.

## Greenlight verdict

Josef explicitly widened the playtest-kit runway beyond v0.
The next follow-through after the cohesive v0 surface-hardening pass should be a richer prepared-base fixture family, not a timid one-off save.

## Why this packet exists

Josef wants a genuinely grand playtest harness where the important Basecamp-management and bandit-interplay seams are ready to probe without rebuilding a camp from rubble every time.
So this packet focuses on rich prepared fixtures: structural footing in captured saves, then light live restage on top.

## Scope

1. Build a small family of rich prepared-base fixtures for current Basecamp-management and bandit-interplay probing.
2. Each shipped playtest-kit fixture should start management-ready enough to matter, including the relevant base structures and supplies rather than a half-empty shell.
3. The intended prepared footing includes, where appropriate for the fixture:
   - Basecamp zone
   - fireplace
   - smart zones
   - locker zones
   - patrol zones
   - armed NPCs
   - food / supplies
   - sane camp-management-ready baseline state
4. The player fixture should carry both clairvoyance mutations so live debugging/readability is less blind and less annoying.
5. Use a hybrid method: captured save fixtures for structural camp footing, plus small live/debug/harness restage helpers for last-mile scenario differences.
6. Keep the fixture family named and intentional rather than multiplying dozens of near-duplicate saves.

## Non-goals

- a universal custom-map empire
- hand-editing arbitrary save guts when a captured fixture plus small restage works
- fresh bandit or Basecamp mechanics
- replacing all playtest setup with one giant monolithic world
- pretending every single future scenario must be fully pre-baked in this packet

## Success shape

This packet is good enough when:
1. a small named family of rich prepared-base fixtures exists for current management/interplay probing
2. the fixtures are meaningfully management-ready rather than empty camp shells
3. the player-side debugging/readability footing includes both clairvoyance mutations
4. the fixture method is honest and reusable: captured save for structure, live restage for variants
5. the scenario family is reviewer-readable enough that Schani, Andi, and Josef can tell which fixture is for which kind of playtest without folklore
6. the packet says plainly what still remains manual, brittle, or not yet fixture-backed
7. the slice stays about rich prepared fixtures, not a generic world-authoring platform

## Validation expectations

This is primarily fixture/harness work.
Validation should therefore bias toward:

- fresh current-build reinstall/load proof for each shipped fixture
- reviewer-readable artifact evidence that the important camp footing is actually present
- narrow deterministic checks only for any code path that truly changes
- honest notes when a fixture still relies on a captured save instead of a declarative builder

## Review questions this packet should answer

- Can Josef load directly into a meaningful camp-management / bandit-interplay situation without camp-prep busywork?
- Do the fixtures expose the intended base structures, zones, and armed-NPC footing clearly enough for real playtesting?
- Does the clairvoyance-enabled player fixture materially improve debug/readability work?
- Did the packet create a useful fixture family instead of sprawling into save-hoarding chaos?

## Skill sync requirement

Any prepared fixtures, fixture naming conventions, reload flows, or clarity/debugging rules that land from this packet should also be reflected in the C-AOL harness skill instead of staying trapped in packet docs alone.
For the broader generic skill package on this surface, Andi should draft the additions and Frau Knackal should review the skill before it is treated as settled.

## Position in the sequence

This packet sits behind playtest kit v0.
V0 should first make the surface reliable, reloadable, and readable.
V1 then deepens that footing into a rich prepared-base fixture family.
