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
- [x] Exact-word `craft` trigger tests exist and pass.
- [x] `witchcraft` / substring false-positive regression test exists and pass.
- [ ] Craft ambiguity / blocker / quantity parsing tests exist and pass.
  - exact-word trigger, quantity parsing, ordered multi-word preference, and ambiguity coverage are in place
  - blocked craft query coverage still needs a deterministic test
- [x] Touched code compiles cleanly.
- [x] Small PR package is explainable without dragging LLM design into the story.

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
- [ ] Run the spoken camp craft smoke packet below and note anything stupid.
- [ ] Before calling the upstream deterministic PR slice ready, do a small hand test with that slice in place and confirm the game still launches and loads a save/world cleanly.

### Spoken camp craft smoke packet (Josef)
Use a camp with a bulletin board and at least one NPC who can plausibly craft.  Try these in normal play, not in a sterile lab if avoidable.

1. `craft knife`
   - expected: spoken craft path triggers and produces a board request / launch / blocker response
   - suspicious: gets ignored, routed as unrelated dialogue, or picks something that is not a knife
2. `craft five bandages`
   - expected: quantity survives, and the request reflects 5 rather than silently collapsing to 1
   - suspicious: quantity is dropped or bark text disagrees with the board entry
3. `witchcraft knife`
   - expected: **no** craft routing, no new board request, no substring false positive
   - suspicious: any craft/job gets queued from this
4. `craft boiled`
   - expected: camp asks for a clearer target instead of guessing among multiple "boiled ..." recipes
   - suspicious: silently chooses one recipe anyway
5. `craft boiled bandages`
   - expected: specific phrase beats the generic `bandages` fallback and resolves to boiled makeshift bandages
   - suspicious: plain bandages get chosen instead
6. If a request lands blocked, read the bark + board entry
   - expected: failure explains itself well enough to recover
   - suspicious: vague "cannot start" wording with no actionable reason

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
