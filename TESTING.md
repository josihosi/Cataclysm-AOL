# TESTING

This file is the canonical testing ledger for the current Cataclysm-AOL stretch.
It separates what Schani can verify alone from what still needs Josef’s hands/eyes.

If a task is supposedly deterministic, it should usually show up here with a deterministic check.
If a task is about feel, weirdness, or real gameplay sanity, it should usually show up here as Josef testing.

---

## Stage meanings
- **DETERMINISTIC TESTS** — compile/tests/parser checks/unit coverage that should pass before playtesting.
- **AGENT TESTING** — Schani/Andi startup/save-load/log sanity and any reliable in-game checks they can run themselves.
- **JOSEF TESTING** — real gameplay, UX, edge-case, and "does this feel stupid?" checks.
- **TWEAK** — follow-up pass after either kind of testing finds something worth fixing.

## Close-out order for almost-finished features
For near-finish work, the intended order is:
1. **Finish line** — implement the current close-out item.
2. **Deterministic tests** — write/update deterministic tests and get them passing.
3. **Andi self-check** — compile, launch/load, and do any cheap/reliable self-checks he can perform directly.
4. **Schani review + smoke/play test** — Schani owns the higher-trust review layer and should run the meaningful smoke/play checks with the broader Discord/session context in mind.
5. **Josef final play test / signoff** — when human feel/judgment is still needed, Josef gets the final targeted packet.
6. **Tweak** — expect at least one tweak round after Josef testing; game work is feelings-heavy and rarely ends in a single perfect pass.

An item is not really "done" just because deterministic tests or Andi self-checks passed. If Schani review or Josef signoff is the next gate, the item stays the active finish line until that handoff is prepared and completed.

---

## Current agent-side checks

### Deterministic upstreamable PR slice
- [x] Exact-word `craft` trigger tests exist and pass.
- [x] `witchcraft` / substring false-positive regression test exists and pass.
- [x] Craft ambiguity / blocker / quantity parsing tests exist and pass.
- [x] Touched code compiles cleanly.
- [x] Small PR package is explainable without dragging LLM design into the story.
- [x] Current HEAD (`696f5c8b61`) passed fresh `make -j4 tests`, `./tests/cata_test "[camp][basecamp_ai]"`, `make -j4 TILES=1 cataclysm-tiles`, and `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'` with zero recorded debug popups (`.userdata/dev/harness_runs/20260404_162509`).

### Movement-system work
- [x] Local tactical `move=<dx>,<dy> <state>` parser/tests exist and pass.
- [x] `wait_here` / `hold_position` suffix behavior is preserved exactly after the syntax change.
- [x] Existing pathing / target-tile behavior still works after replacing only the LLM-facing coordinate payload.
- [x] Overmap `dx` / `dy` parser tests exist and pass.
- [x] Overmap `stay` / `move_omt dx=<signed_int> dy=<signed_int>` token parser tests exist and pass.
- [x] Overmap token formatter tests exist and round-trip through the shared parser/resolver.
- [x] Overmap origin+delta resolution reuses the same signed axis convention and preserves `stay`.
- [x] Snapshot/prompt axis hints are present for the delta contract.
- [x] Malformed movement falls back safely.

### Basecamp work on `dev`
- [x] Relevant deterministic tests exist for the new routing/token layer.
- [x] Structured `craft=<query>` / `show_board` / `show_job=<id>` / `job=<id>` / `delete_job=<id>` token parsers are covered by deterministic tests.
- [x] Structured batch board tokens (`launch_ready_jobs`, `retry_blocked_jobs`, `clear_archived_jobs`) are covered by deterministic tests.
- [x] Crafting-request details expose a compact deterministic handoff snapshot with stable request facts (`id` / `query` / `count` / `source`) plus exact board/detail/follow-up tokens, and the formatter is covered by deterministic tests.
- [x] Board handoff snapshots expose stable active/archive counts plus per-job detail/action tokens, and the formatter is covered by deterministic tests.
- [x] The Basecamp craft-handoff snapshot now loads from the shared prompt-template path (`data/llm_prompts` with `config/llm_prompts` overrides) while preserving the deterministic formatter contract via the same tests.
- [x] `dev` build compiles.
- [x] Game launches.
- [x] Known save loads successfully.
- [x] No new debug-log regressions were introduced by the slice.

---

## Current Schani-side checks

### Pending now
- [ ] Run the spoken camp craft smoke packet below on HEAD `696f5c8b61` and note anything stupid before this goes to Josef.
- [ ] Specifically sanity-check the latest spoken craft-resolution close-out on current HEAD:
  - `craft 5 makeshift bandages` should take the positive deterministic path and pin/start cleanly
  - `craft boiled` should clarify instead of guessing
  - `craft 5 bandages` should stay deterministic blocked/no-crash and explain the resolved recipe/blocker clearly
  - live craft-board replies should still lead with the human request subject and keep the request id as trailing detail instead of sounding like a filing cabinet
- [ ] Before calling the upstream deterministic PR slice ready, do a small hand test with that slice in place and confirm the game still launches and loads a save/world cleanly.

### Spoken camp craft smoke packet (Schani first)
Use a camp with a bulletin board and at least one NPC who can plausibly craft.  Try these in normal play, not in a sterile lab if avoidable.

Current narrow live-bed target when available:
- `dev` / `Sandy Creek`
- 2 waiting camp NPCs
- enough stock for makeshift bandages
- not enough stock for plain bandages or boiled bandages
- specifically verify on HEAD `696f5c8b61`:
  - `craft 5 makeshift bandages` takes the positive deterministic path and pins/starts cleanly
  - `craft boiled` still clarifies instead of guessing
  - `craft 5 bandages` stays blocked/no-crash
  - bark + board/details surface the resolved recipe and blocker state clearly enough to understand what happened

1. `craft 5 makeshift bandages`
   - expected: positive deterministic path; request pins/starts cleanly and stays understandable in bark/board text
   - suspicious: no-match, wrong recipe choice, dropped quantity, or weirdly bureaucratic bark
2. `craft 5 bandages`
   - expected: quantity survives, no crash occurs, and the blocked path explains the matched recipe + blocker clearly
   - suspicious: crash, quantity collapse, silent no-match, or blocker wording too vague to recover from
3. `craft boiled`
   - expected: camp asks for a clearer target instead of guessing among multiple `boiled ...` recipes
   - suspicious: silently chooses one recipe anyway or gives a generic non-answer
4. `witchcraft knife`
   - expected: **no** craft routing, no new board request, no substring false positive
   - suspicious: any craft/job gets queued from this
5. `craft boiled bandages`
   - expected: specific phrase beats the generic `bandages` fallback and resolves to boiled makeshift bandages
   - suspicious: plain bandages get chosen instead
6. If a request lands blocked, read the bark + board entry/details
   - expected: failure explains itself well enough to recover, including the resolved recipe when it differs from what was heard
   - suspicious: vague `cannot start` wording with no actionable reason or missing matched-recipe detail

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
- [x] code compiles
- [x] relevant deterministic tests pass
- [x] no LLM dependency is required
- [x] game launches successfully with the PR slice in place
- [x] save/world load succeeds with the PR slice in place
- [ ] the actual deterministic feature gets a small manual smoke/play test
- [x] scope is small and reviewable
- [x] PR description matches the code honestly
- [ ] Josef is not embarrassed by the behavior

### Basecamp work on `dev`
The actual finish line remains:
- [x] implemented on `dev`
- [x] compiles
- [x] game launches
- [x] save loads successfully
- [x] zero new debug messages / zero crashes
- [ ] Josef gameplay pass completed
- [ ] tweak round completed if needed

---

## Notes / current annoyances
- Full `tests/cata_test` linking on this Mac has recently hit local framework/library link trouble (SDL / SDL_ttf / CoreFoundation / FreeType). Treat that as environment work to revisit, not as an excuse to skip logic tests entirely.
- Keep manual test packets short and concrete: what changed, what to try, expected result, suspicious edge cases.
