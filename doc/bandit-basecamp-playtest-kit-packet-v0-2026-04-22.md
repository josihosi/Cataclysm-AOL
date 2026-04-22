# Bandit + Basecamp playtest kit packet v0 (2026-04-22)

Status: ACTIVE / GREENLIT.

## Greenlight verdict

Josef explicitly greenlit one larger cohesive pre-playtest infrastructure packet instead of another chain of tiny helper slices.
The first-pass encounter/readability packet is now closed with an honest verdict: real pressure exists, but the immediate read is weak enough that the missing value is clearly playtest-surface quality rather than another fresh mechanics debate.
This packet is therefore the active follow-through.

## Why this packet exists

Andi is burning through small helper packets quickly, and Josef has a short upcoming playtest window.
So the honest next auxiliary target is not another miniature harness nicety in isolation, but one coherent playtest-kit pass that makes the live surface faster to reload, easier to read, less annoying to rerun, and more reusable across a few nearby scenario variants.

## Scope

1. Keep the already-landed helper seam `bandit.basecamp_named_spawn_mcw` and harden it for repeated operator use rather than reopening setup folklore.
2. Add a bounded repeatability soak for the current helper path so later playtest runs can trust the restage seam instead of wondering whether it flakes.
3. Improve the screen-first artifact summary so the meaningful proof for these live probes is easier to read than the current generic artifact verdict.
4. Package probe auto-cleanup so the helper path does not keep leaving behind stale live windows as operator litter.
5. Create one purpose-built fast-reload playtest fixture/save pack for current bandit + Basecamp probing, with a clear spawn anchor and a small named scenario set that is easy for Josef to reload during later manual playtesting.
6. Keep the scenario set small and intentional, for example day/night/bad-weather/follower-present variants, rather than widening into a giant sandbox.

## Non-goals

- a general custom-map framework
- a generic scenario-authoring engine
- fresh bandit or Basecamp mechanics
- reopening zoning-mechanics arguments
- an infinite "whatever we want" debug sandbox
- replacing every existing fixture with one huge authored world

## Success shape

This packet is good enough when:
1. the current named-bandit helper path has honest repeatability evidence instead of one lucky run
2. the artifact/report surface for that helper path is screen-first and reviewer-readable enough that later playtest discussion does not depend on folklore
3. the helper path cleans up after itself well enough that repeated probes do not leave stale game windows or obvious session clutter behind
4. one purpose-built playtest fixture/save pack exists with a fast reinstall/load path for current bandit + Basecamp probing
5. that fixture/save pack exposes a small named scenario family suitable for Josef playtesting and debugging, not just one brittle operator-only restage ritual
6. the packet says plainly what still remains ugly or manual instead of laundering it into magic
7. the slice stays cohesive and bounded rather than mutating into a general harness/world-authoring rewrite

## Validation expectations

This is primarily harness/helper/fixture work, so validation should stay proportionate:

- use fresh current-build live proof for helper repeatability and any new fixture-backed scenario paths
- preserve reviewer-readable screen artifacts where live readability matters
- add deterministic coverage only for any narrow code paths that actually change
- if the fixture path depends on a captured save, say so plainly instead of pretending it is a fully declarative authored-world system

## Review questions this packet should answer

- Is the restage/reload surface now pleasant enough that Josef can use it repeatedly without ritual annoyance?
- Do the artifacts make the important live truth obvious without reading tea leaves in generic logs?
- Is the dedicated playtest fixture genuinely useful for debugging and scenario variation, or is it still too brittle to trust?
- Did the packet stay a coherent pre-playtest kit, or did it start trying to become a whole harness platform?

## Skill sync requirement

Any helper, fixture, artifact/report surface, or operator workflow that lands from this packet should also be folded into the C-AOL harness skill instead of living only in repo canon or chat memory.
For the broader generic skill package on this surface, Andi should draft the additions and Frau Knackal should review the skill before it is treated as settled.

## Position in the sequence

This is now the active cohesive infrastructure packet behind the closed first-pass encounter/readability lane.
Its job is to make the near-term human playtest window smoother now that the first live readability truth is on the table.
