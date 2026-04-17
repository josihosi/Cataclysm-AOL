# Locker combat-oriented policy direction (2026-04-09)

Status: parked future contract

This packages Josef's correction to the locker direction after the heavy weird-garment safety work.

## Core direction

Keep the current garment-safety work, but shift future locker policy toward sensible guard/combat outfits instead of spending all attention on artisanal clothing edge cases.

The key idea is:
- preserve the weird-garment safety wins already earned
- focus future policy/testing more on what players actually want guards and camp followers to wear most of the time

## Default normal useful kit

Future locker behavior should strongly support sensible common gear like:
- pants
- shirt
- vest
- boots
- socks
- gloves
- helmet
- goggles
- mask / dust mask
- backpack / sling pack
- melee weapon
- sidearm
- belt
- holster when sidearm is enabled

## Bulletin-board / control-surface direction

Add a bulletproof toggle on the bulletin-board / locker settings surface.

When bulletproof mode is enabled:
- NPCs should prefer ballistic / bulletproof gear more strongly
- ballistic vest handling should become explicit rather than accidental
- plate handling should become explicit too
- damaged or ruined (`XX`) ballistic components should be replaced sensibly

## Full battle-suit preference

If a clearly superior full-body battle/protective suit is available and sane for the active policy, it should generally be preferred over splitting into worse piecemeal junk.

## Testing direction

Future deterministic tests should lean more toward:
- sensible guard/combat clothing
- gloves / belts / masks / holsters
- ballistic toggle behavior
- ballistic vest + plate handling
- fancy full armor battle suits

And less toward endlessly expanding exotic-clothing law for its own sake.

## Important constraint

This does not invalidate or undo the current weird-garment safety work.
That work stays valuable.
The change is in future emphasis and future policy direction.

## Non-goal

This is not a whole combat doctrine engine.
It is a more combat-oriented locker policy plus clearer player-facing controls.
