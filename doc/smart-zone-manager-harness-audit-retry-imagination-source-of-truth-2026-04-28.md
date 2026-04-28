# Smart Zone Manager harness-audit retry imagination source of truth - 2026-04-28

Status: SOURCE OF TRUTH

Plan item: `Smart Zone Manager harness-audit retry packet v0`

## Finished scene

A player with a normal, loadable basecamp save opens Zone Manager, invokes the Smart Zone Manager generation action, and then sees a useful generated basecamp layout instead of one suspicious pile of zones pretending to be design.

The important scene is not “the tests know rectangles exist.” It is this: the live game accepts the player action, creates the intended zones in the player-visible Zone Manager state, keeps zones that should be separate on separate reserved tiles/rects, and shows the same generated layout again after close/reopen or save/reload.

## What must feel true

- The Smart Zone Manager behaves like a one-shot basecamp helper, not an invisible script that only leaves comforting metadata behind.
- The player-visible layout is legible: books/manuals/food/drink/storage-style zones land in intended places instead of collapsing into one fake mega-tile.
- Expected overlaps are explicit and intentional; accidental overlaps are treated as bugs, not “eh, rectangles are hard.”
- The generated layout survives the immediate product loop: create, inspect, close/reopen, still there.
- The proof packet is small enough to review without drowning in screenshots.

## What would be hollow

- Startup/load succeeds, but no Smart Zone generation happens.
- Deterministic geometry tests pass, but the live UI path never proves generated player-visible zones.
- A fixture already contains the target zones, and the proof quietly launders setup metadata as live generation.
- Screenshots show a menu but not generated positions/layout.
- OCR or row counts replace exact zone rects/coords/options when structured saved-zone metadata is available.
- The old contaminated McWilliams macro is reused as closure because it is convenient.
- A non-loadable clean profile is retried as if “Dunn has no characters to load” were a feature test. Na bravo.

## Proof shape

The honest proof starts with a current-runtime loadable profile, then follows the player-facing chain:

1. enter gameplay;
2. open Zone Manager through the live UI path;
3. invoke Smart Zone Manager generation through the live UI path;
4. capture structured generated-zone state after creation;
5. close/reopen or save/reload and capture the same generated layout again;
6. compare exact zone names/types/options/rects/coords and overlap matrix against the intended layout contract.

Screenshots are supporting witnesses only. The closure should rest on live-derived structured zone metadata plus a causal UI/action chain.

## Review question

If this proof shipped, would Josef believe the player can actually run Smart Zone Manager and see the corrected layout in-game, or did we merely build a prettier harness shrine next to the bug?
