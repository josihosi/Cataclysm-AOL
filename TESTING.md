# TESTING

This file is the canonical testing ledger for the current Cataclysm-AOL stretch.
It separates what Schani can verify alone from what still needs Josef’s hands/eyes.

If a task is supposedly deterministic, it should usually show up here with a deterministic check.
If a task is about feel, weirdness, or real gameplay sanity, it should usually show up here as Josef testing.

---

## Stage meanings
- **AGENT TESTING** — compile/tests/startup/save-load/log sanity that Schani should run.
- **JOSEF TESTING** — real gameplay, UX, edge-case, and “does this feel stupid?” checks.
- **TWEAK** — follow-up pass after either kind of testing finds something worth fixing.

---

## Current agent-side checks

### Deterministic upstreamable PR slice
- [ ] Exact-word `craft` trigger tests exist and pass.
- [ ] `witchcraft` / substring false-positive regression test exists and pass.
- [ ] Craft ambiguity / blocker / quantity parsing tests exist and pass.
- [ ] Touched code compiles cleanly.
- [ ] Small PR package is explainable without dragging LLM design into the story.

### Movement-system work
- [ ] Local tactical `move=<dx>,<dy> <state>` parser/tests exist and pass.
- [ ] `wait_here` / `hold_position` suffix behavior is preserved exactly after the syntax change.
- [ ] Existing pathing / target-tile behavior still works after replacing only the LLM-facing coordinate payload.
- [ ] Overmap `dx` / `dy` parser tests exist and pass.
- [ ] Malformed movement falls back safely.

### Basecamp work on `dev`
- [ ] Relevant deterministic tests exist for the new routing/token layer.
- [ ] `dev` build compiles.
- [ ] Game launches.
- [ ] Known save loads successfully.
- [ ] No new debug-log regressions were introduced by the slice.

---

## Current Josef-side checks

### Pending now
- [ ] Live-check the follower snapshot legend change in-game:
  - target legend shows attitude (`friendly` / `neutral` / `hostile`) plus threat
  - lettered-target wording still feels sensible in real use
- [ ] When the exact-word `craft` slice lands, try representative spoken camp craft commands and note:
  - does the intended craft trigger reliably?
  - do obvious false positives stay dead?
  - does the resulting board/job behavior feel natural enough to keep?
- [ ] Before calling the upstream deterministic PR slice ready, do a small hand test with that slice in place and confirm the game still launches and loads a save/world cleanly.

### General human-eye checks
- [ ] Does Basecamp interaction feel clearer rather than more bureaucratic?
- [ ] Do NPC replies / barks remain understandable and not absurdly spammy?
- [ ] Do command failures explain themselves well enough for a player to recover?
- [ ] Does the movement grammar feel easier rather than merely more technical?

---

## Signoff gates

### Upstream deterministic PR slice
Do not call this ready just because the diff exists.
It is ready when:
- [ ] code compiles
- [ ] relevant deterministic tests pass
- [ ] no LLM dependency is required
- [ ] game launches successfully with the PR slice in place
- [ ] save/world load succeeds with the PR slice in place
- [ ] the actual deterministic feature gets a small manual smoke/play test
- [ ] scope is small and reviewable
- [ ] PR description matches the code honestly
- [ ] Josef is not embarrassed by the behavior

### Basecamp work on `dev`
The actual finish line remains:
- [ ] implemented on `dev`
- [ ] compiles
- [ ] game launches
- [ ] save loads successfully
- [ ] zero new debug messages / zero crashes
- [ ] Josef gameplay pass completed
- [ ] tweak round completed if needed

---

## Notes / current annoyances
- Full `tests/cata_test` linking on this Mac has recently hit local framework/library link trouble (SDL / SDL_ttf / CoreFoundation / FreeType). Treat that as environment work to revisit, not as an excuse to skip logic tests entirely.
- Keep manual test packets short and concrete: what changed, what to try, expected result, suspicious edge cases.
