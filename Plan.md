# C-AOL Release Roadmap

## Remaining Work
- README polish and release-note cleanup.
- Release discussion / final packaging sanity check after README edits.
- Decide how LLM-written `wait_here` / `hold_position` should map into in-game behavior.
- Revisit pickup behavior in the presence of zombies: pickup should likely be blocked by panic / being grabbed, not merely by a visible zombie.
- Add short NPC refusal utterances for blocked pickup/action attempts (`No room.`, `Not now.`, etc.).

## Known Caveats To Note For Release
- `wait_here` / `hold_position` handling is still not implemented to the desired level.
- Pickup threat-policy still needs refinement around nearby zombies versus actual panic / grabs.
- Some failed pickup cases still need better NPC-facing feedback instead of silent no-ops.
