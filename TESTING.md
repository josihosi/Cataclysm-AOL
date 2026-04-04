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
- [x] Schani smoke baseline `311c7ab1b7` (main behavior fix `696f5c8b61`) passed the earlier fresh validation packet and live spoken-craft trio.
- [x] Gameplay signoff target `4a39c70ac7` (same spoken-craft behavior plus blocked-bark punctuation trim in `1df9e378c8`) re-passed fresh `make -j4 tests`, `./tests/cata_test "[camp][basecamp_ai]"`, `make version TILES=1 -j4 cataclysm-tiles`, and `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'` with zero recorded debug popups (`.userdata/dev/harness_runs/20260404_171141`).

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
- [x] Small overmap snapshot grid formatter exists for the broader Basecamp AI snapshot.
- [x] Legend output is present-only rather than dumping the whole symbol table.
- [x] Collapsed terrain symbols use lowercase normal / UPPERCASE horde-present variants.
- [x] Deterministic tests cover the snapshot formatter / legend / malformed fallback behavior for the overmap snapshot contract.
- [x] The first live Basecamp planner consumer now carries `planner_move=stay | move_omt dx=<signed_int> dy=<signed_int>` plus the shared overmap snapshot block when `show_board` has a real camp origin.
- [x] Current live-consumer slice re-passed `make -j4 tests cataclysm-tiles`, `./tests/cata_test "[camp][basecamp_ai]"`, and `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'` with zero recorded debug popups (`.userdata/dev/harness_runs/20260404_210854`).
- [x] Schani review re-ran the same compile/test/startup trio on current dirty HEAD and again got zero recorded debug popups (`.userdata/dev/harness_runs/20260404_213253`).

### Basecamp work on `dev`
- [x] Relevant deterministic tests exist for the new routing/token layer.
- [x] Structured `craft=<query>` / `show_board` / `show_job=<id>` / `job=<id>` / `delete_job=<id>` token parsers are covered by deterministic tests.
- [x] Structured batch board tokens (`launch_ready_jobs`, `retry_blocked_jobs`, `clear_archived_jobs`) are covered by deterministic tests.
- [x] Crafting-request details expose a compact deterministic handoff snapshot with stable request facts (`id` / `query` / `count` / `source`) plus exact board/detail/follow-up tokens, and the formatter is covered by deterministic tests.
- [x] Board handoff snapshots expose stable active/archive counts plus per-job detail/action tokens, and the formatter is covered by deterministic tests.
- [x] The live `show_board` handoff now prepends planner-move contract text plus the shared overmap snapshot block when the camp origin is available, and deterministic tests cover that consumer path.
- [x] The Basecamp craft-handoff snapshot now loads from the shared prompt-template path (`data/llm_prompts` with `config/llm_prompts` overrides) while preserving the deterministic formatter contract via the same tests.
- [x] `dev` build compiles.
- [x] Game launches.
- [x] Known save loads successfully.
- [x] No new debug-log regressions were introduced by the slice.

---

## Current Schani-side checks

### Done / handed upward
- [x] Run the spoken camp craft smoke packet below on the Schani smoke baseline `311c7ab1b7` and note anything stupid before this goes to Josef.
- [x] Specifically sanity-check the latest spoken craft-resolution close-out on current HEAD:
  - [x] `craft 5 makeshift bandages` takes the positive deterministic path and pins cleanly
  - [x] `craft boiled` clarifies instead of guessing
  - [x] `craft 5 bandages` stays deterministic blocked/no-crash and explains the resolved recipe/blocker clearly
  - [x] live craft-board replies still lead with the human request subject and keep the request id as trailing detail instead of sounding like a filing cabinet
- [x] Before calling the upstream deterministic PR slice ready, do a small hand test with that slice in place and confirm the game still launches and loads a save/world cleanly.
- [x] Review the movement-system `show_board` handoff on current HEAD and decide whether any broader agent-side smoke remains before Josef handoff:
  - [x] re-ran `make -j4 tests cataclysm-tiles`
  - [x] re-ran `./tests/cata_test "[camp][basecamp_ai]"`
  - [x] re-ran `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
  - [x] no new crash/debug-popup issue appeared, so the remaining gate is Josef readability/feel rather than more agent-side archaeology

### Spoken camp craft smoke packet (done by Schani)
Use a camp with a bulletin board and at least one NPC who can plausibly craft.  Try these in normal play, not in a sterile lab if avoidable.

Completed live-bed target:
- `dev` / `Sandy Creek`
- Schani smoke baseline `311c7ab1b7` (main behavior fix `696f5c8b61`)
- current gameplay signoff target is `4a39c70ac7`, which only adds the tiny blocked-bark punctuation trim in `1df9e378c8` plus doc alignment and has already re-passed agent-side compile/test/startup checks
- enough stock for makeshift bandages
- not enough stock for plain bandages or boiled bandages
- verified live:
  - `craft 5 makeshift bandages` takes the positive deterministic path and pins cleanly
  - `craft boiled` clarifies instead of guessing
  - `craft 5 bandages` stays blocked/no-crash
  - bark + log trace surface the resolved recipe and blocker state clearly enough to understand what happened

Observed summary:
1. `craft 5 makeshift bandages`
   - result: positive deterministic path; bark pinned the request cleanly; debug trace resolved to `makeshift bandage`
2. `craft 5 bandages`
   - result: quantity survived, no crash, blocked path named the matched recipe and the real storage/tool blocker
3. `craft boiled`
   - result: deterministic clarification instead of guessing, with a boiled-* candidate list including the expected bandage candidate
4. `witchcraft knife`
   - already covered by deterministic regression tests; no extra live rerun needed for this close-out gate
5. `craft boiled bandages`
   - still belongs in later human-feel smoke, but no longer blocks the deterministic spoken-craft close-out itself
6. If a request lands blocked, read the bark + board entry/details
   - current state: blocked bark/log result is good enough for Josef smoke; any remaining ugliness is wording consistency, not correctness archaeology

## Current Josef-side checks
- [x] Run the narrow final smoke/signoff trio for spoken camp-craft on gameplay signoff target `4a39c70ac7`:
  - [x] `craft 5 makeshift bandages`
  - [x] `craft boiled`
  - [x] `craft 5 bandages`
- [x] Judge whether bark + board/detail wording feels human/clear enough, especially for blocked requests where the heard phrase and resolved recipe differ.
- [x] During the blocked probe, just eyeball that the tiny punctuation fix from `1df9e378c8` really killed silliness like `tools..`; if the live text still manages to look stupid, that becomes the tweak note.
- [ ] Run the narrow movement-system board-handoff packet on current `dev` / `Sandy Creek`:
  - [ ] ask a camp NPC `show me the board` (or `what's on the board`) and confirm the reply leads with `planner_move=stay | move_omt dx=<signed_int> dy=<signed_int>` plus the overmap block before the job list
  - [ ] judge whether the 5x5 snapshot / `dx` / `dy` hints make movement reasoning easier instead of merely more technical
  - [ ] confirm the active/archive job lines still read clearly and are not buried by the new planning context

### General human-eye checks
- [ ] Does Basecamp interaction feel clearer rather than more bureaucratic?
- [ ] Do NPC replies / barks remain understandable and not absurdly spammy?
- [ ] Do command failures explain themselves well enough for a player to recover?
- [ ] Does the movement grammar feel easier rather than merely more technical?

---

## Signoff gates

### Upstream deterministic PR slice (parked reference)
The local technical slice reached the intended finish state, but the actual upstream PR attempt is parked/closed for social-review reasons.
Keep this here as a record of what passed locally.
- [x] code compiles
- [x] relevant deterministic tests pass
- [x] no LLM dependency is required
- [x] game launches successfully with the PR slice in place
- [x] save/world load succeeds with the PR slice in place
- [x] the actual deterministic feature got a small manual smoke/play test
- [x] scope is small and reviewable
- [x] local PR description was eventually corrected to match the feature premise honestly
- [x] Josef gameplay signoff on the feature behavior happened before the upstream close

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
- Direct deterministic `show_board` responses do not currently append a fresh block to `config/llm_intent.log`; if later inspection wants log-based artifacts for that path, it needs either an explicit logging hook or a different artifact source.
- Keep manual test packets short and concrete: what changed, what to try, expected result, suspicious edge cases.
