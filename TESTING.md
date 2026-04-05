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

### Harness observability / deterministic probe slice
- [x] Structured board/base-AI snapshot replies now have a reliable artifact path: with `DEBUG_LLM_INTENT_LOG` enabled, deterministic `show_board` / `show_job` append explicit `camp board reply` / `camp job reply` blocks to `config/llm_intent.log`; dirty HEAD `1a54665658-dirty` re-passed `make -j4 tests cataclysm-tiles`, `./tests/cata_test "[camp][basecamp_ai]"`, and startup harness `dev` / `Sandy Creek` (`.userdata/dev/harness_runs/20260404_230538`).
- [x] Live Basecamp hearing is now provable in logs too: after a sufficiently long post-load settle, three fresh-start GUI probes with unique utterances (`show me the board please`, `what's on the board`, `craft boiled please`) each appended `camp heard Bruna Priest (2)` plus the unique `utterance=...` line to `config/llm_intent.log`.
- [x] The harness/live probe flow now distinguishes `say sentence` from `yell a sentence` explicitly and reproducibly: on `dev` / `Sandy Creek`, after the long post-load settle, `C` → `b` opens `say a sentence` and `C` → `y` opens `yell a sentence`; live probes with unique `show me the board ...` utterances appended the expected `camp heard ... / utterance=...` evidence to `config/llm_intent.log`.
- [x] Probe reports keep evidence classes separate: on-screen behavior, deterministic test results, and artifact/log visibility.
- [x] A successful live probe now records where the evidence landed instead of guessing: the `yellprobe 2345` board query was confirmed in both places — on-screen message log (`You tell 'show me the board'`, followed by the NPC bark beginning `Here is the current...`) and `config/llm_intent.log` (`camp heard ...`, `response ...`, `say ...`).
- [x] Current `dev` / `Sandy Creek` startup reality is documented: `game::load: Finalizing end` is too early as a live-input marker; this bed needs a much longer post-load settle before GUI probes are trustworthy.

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
- [x] Schani later caught a stale/brittle uppercase-horde expectation in the overmap snapshot tests; fixed in `88f2e3eeb7` and the targeted suite re-passed (`266 assertions in 1 test case`).
- [x] A later forced fresh rebuild on dirty HEAD `58c620d098-dirty` exposed one more structured-board assertion that still assumed lowercase terrain letters on the camp/road row; the check now looks at the intended row features case-insensitively, and `./tests/cata_test "[camp][basecamp_ai]"` re-passed on the rebuilt binary (`266 assertions in 1 test case`).
- [x] Technical path split is now explicit rather than speculative: the richer `planner_move` + overmap snapshot block is confirmed in the structured/internal `show_board` / LLM-side handoff path, while live natural speech `show me the board` currently returns the older concise board-summary bark instead.
- [x] Product shape is clarified: keep the human-facing spoken path concise; inject the richer 5x5 overmap snapshot plus present-only legend into the LLM snapshot/prompt path when deterministic handling falls through.

### Basecamp work on `dev`
- [x] Relevant deterministic tests exist for the new routing/token layer.
- [x] Structured `craft=<query>` / `show_board` / `show_job=<id>` / `job=<id>` / `delete_job=<id>` token parsers are covered by deterministic tests.
- [x] Structured batch board tokens (`launch_ready_jobs`, `retry_blocked_jobs`, `clear_archived_jobs`) are covered by deterministic tests.
- [x] Crafting-request details expose a compact deterministic handoff snapshot with stable request facts (`id` / `query` / `count` / `source`) plus exact board/detail/follow-up tokens, and the formatter is covered by deterministic tests.
- [x] Board handoff snapshots expose stable active/archive counts plus per-job detail/action tokens, and the formatter is covered by deterministic tests.
- [x] Human-facing bark formatting now has deterministic coverage for the current close-out pass: spoken status keeps request ids trailing instead of leading, blocked status can still surface the matched recipe, and the short board summary stays concise/human.
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
- [x] Review the current Basecamp AI bark/feel pass on the current dirty tree and decide whether any broader agent-side smoke remains before Josef handoff:
  - [x] latest bark-tweak dirty tree was rebuilt fresh enough to kill the stale-binary lie (`make -j4 TILES=1 cataclysm-tiles`, `make -j4 cataclysm.a`, then top-level `make -j4 tests`)
  - [x] latest bark-tweak dirty tree re-passed `./tests/cata_test "[camp][basecamp_ai]"` (`269 assertions in 1 test case`) on version `7879d7b07b-dirty`
  - [x] latest bark-tweak dirty tree re-passed `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'` on `.userdata/dev/harness_runs/20260405_014335`
  - [x] deterministic bark formatter coverage now exists for trailing-id spoken status and the concise short board-summary bark
  - [x] no new crash/debug-popup issue appeared (`.userdata/dev/harness_runs/20260405_001058` on the original bark-pass dirty tree, rechecked again at `.userdata/dev/harness_runs/20260405_003604` after the later docs-only head churn, refreshed on `.userdata/dev/harness_runs/20260405_010429` after the latest bark-tweak sweep, and freshly revalidated on current dirty `7879d7b07b` at `.userdata/dev/harness_runs/20260405_014335`)
  - [x] product shape clarified: live spoken `show me the board` stays the concise board-summary bark path, while the richer planner snapshot belongs in the structured/internal / LLM snapshot path
  - [x] human-facing status bark now suppresses job-number-forward phrasing where possible by moving `(#id)` to trailing detail instead of leading with `#id ...`
  - [x] latest bark-tweak sweep also rechecked the softer board/clear phrasing (`Board's got ...`, `Cleared old ... off the board.`) so Josef is judging the current wording, not an older packet
  - [x] Josef handoff packet is prepared/sent with the exact short live tone/feel checks below
  - [ ] next real check is the human-facing one: verify the current bark pass sounds informal, camp-like, and free of job-number / filing-cabinet tone
  - [ ] later, when the LLM-side path returns to the top of the queue: prove the 5x5 overmap snapshot plus present-only legend enters the prompt when deterministic handling falls through

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
- [x] Do **not** run the old movement-system board packet verbatim anymore; it assumed the richer planner snapshot lived in the live spoken board-reply path.
- [x] Product shape clarified: live natural speech `show me the board` / `what's on the board` stays the concise human-facing bark path.
- [ ] Run the current bark/feel packet on the current dirty `dev` tree (agent-validated by `.userdata/dev/harness_runs/20260405_001058`, rechecked again at `.userdata/dev/harness_runs/20260405_003604` after the later docs-only head churn, refreshed on `.userdata/dev/harness_runs/20260405_010429` after the latest bark-tweak sweep, and freshly revalidated on current dirty `7879d7b07b` at `.userdata/dev/harness_runs/20260405_014335`):
  - [ ] `show me the board` or `what's on the board` → should stay a short human board summary like `Board's got ...`, not the structured `planner_move` / overmap snapshot block
  - [ ] `status of bandages` (or another pinned/blocked request) → should name the subject first, keep any `(#id)` trailing, and land closer to `... is pinned, waiting on your go-ahead` / `... is blocked — ...` than the old filing-cabinet bark
  - [ ] `craft 5 bandages` on the current low-stock save → should still explain the real blocker clearly, but in a camp voice rather than a spreadsheet voice (`it's blocked`, not `it is blocked:` bookkeeping sludge)
  - [ ] `clear request #...` on an old completed/cancelled entry → should sound like clearing old work off the board, not like archiving paperwork
  - [ ] Flag any line that still sounds like a bored quartermaster reading a ledger out loud
- [ ] Next validation target is the LLM-side path: prove the richer `planner_move` + 5x5 overmap snapshot + present-only legend enters the prompt when deterministic handling falls through.
- [ ] Only after that LLM-side path is wired/testable should anyone judge readability/coverage of the richer planner snapshot block.

### General human-eye checks
- [ ] Does Basecamp interaction feel clearer rather than more bureaucratic?
- [ ] Do NPC replies / barks remain understandable and not absurdly spammy?
- [ ] Do command failures explain themselves well enough for a player to recover?
- [ ] Does the movement grammar feel easier rather than merely more technical?

---

## Locker Zone v1 — groundwork + classifier/candidate slice

### Deterministic contracts
- Added `camp_locker_policy_serialization` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_item_classification` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_zone_candidate_gathering` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_loadout_planning` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_service_equips_upgrades_and_returns_replaced_gear` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_zone_candidate_gathering_respects_reservations` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_downtime_queue_processes_one_worker_at_a_time` in `tests/faction_camp_test.cpp`.
- Added `camp_locker_downtime_rehydrates_transient_assignees` in `tests/faction_camp_test.cpp`.
- Intended contract now covers:
  - `CAMP_LOCKER` zone type is registered.
  - a basecamp locker policy defaults to all managed slots enabled.
  - toggled-off slots survive basecamp JSON save/load round-trip.
  - representative items classify into the expected managed locker slots (underwear/socks/shoes/pants/shirt/vest/body armor/helmet/glasses/bag/melee/ranged).
  - locker-zone candidate gathering buckets only items on `CAMP_LOCKER` tiles and respects disabled policy slots.
  - locker-zone candidate gathering also respects temporary reservations so the next worker does not target the same candidate blindly.
  - locker loadout planning keeps the best current managed item, marks duplicate current gear for cleanup, equips missing managed categories, selects obvious upgrades only when the candidate clears the slot threshold, and leaves disabled categories out of the plan entirely.
  - the first live locker service helper actually equips an obvious upgrade from the locker zone and returns the replaced managed item back to locker storage.
  - downtime locker orchestration services one worker at a time with cooldown spacing between passes.
  - a transiently empty camp assignee cache can be rehydrated before downtime locker queue sanitization, which is the post-load shape the live save hit.
- 2026-04-05: after a fresh top-level relink (`make -j4 tests cataclysm-tiles`), `./tests/cata_test "[camp][locker]"` passed on dirty `f1e786db48`.
  - result: passed (`94 assertions in 8 test cases`)
  - meaning: the locker groundwork, planner-level duplicate-cleanup/upgrade rules, queue/reservation tail, and transient-assignee rehydration fix all executed on a freshly relinked `cata_test`, not just as compiled object files

### Compile evidence
- 2026-04-05: fresh top-level relink `make -j4 tests cataclysm-tiles`
  - result: passed
  - meaning: the current locker policy/menu/save-load plumbing, classifier/candidate helpers, live locker service hook, queue/reservation path, and post-load assignee-refresh fix all rebuilt cleanly together on this Mac

### Live smoke / harness evidence
- 2026-04-05: earlier groundwork-only startup/load smoke still stands:
  - command: `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
  - result: passed on the fresh post-relink groundwork tree with zero recorded debug popups
  - artifact: `.userdata/dev/harness_runs/20260405_024224`
  - meaning: the menu/save-load groundwork tree launched and loaded the known save cleanly after the forced rebuild
- 2026-04-05: fresh startup harness on the current execution tree now passes on freshly relinked current binaries.
  - commands: `make -j4 TILES=1 cataclysm-tiles`, then `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'`
  - result: passed with zero recorded debug popups on `.userdata/dev/harness_runs/20260405_043857`, `.userdata/dev/harness_runs/20260405_054403`, and the post-assignee-refresh-fix rerun `.userdata/dev/harness_runs/20260405_101148`
  - replaced false alarm: the earlier popup/dismissal failures (`.userdata/dev/harness_runs/20260405_034420`, `.userdata/dev/harness_runs/20260405_034724`, `.userdata/dev/harness_runs/20260405_034951`, `.userdata/dev/harness_runs/20260405_041728`) were not trustworthy evidence against the current source tree because the repo-local `cataclysm-tiles` binary was stale until the explicit `TILES=1` relink
  - pre-slice comparison artifacts still useful: `.userdata/dev/harness_runs/20260405_032319`, `.userdata/dev/harness_runs/20260405_032641`

### Live behavior evidence
- Probe method stayed the same before and after the fix: harness-load `dev` / `Sandy Creek`, wait for normal gameplay, then inject a plain wait-turn packet near `Debug Central` with Peekaboo and inspect the same `config/debug.log` sink.
- Pre-fix evidence on the same save/method: the loaded game only spammed `camp locker: queued Ricky Broughton wake_dirty=false queue_size=1 next_turn=0`, which exposed that the live post-load assignee state was being sanitized away before service.
- 2026-04-05 post-fix rerun on `.userdata/dev/harness_runs/20260405_101148`: a 120-turn wait packet produced real live downtime service for the camp-assigned path.
  - result: passed
  - live behavior meaning: the real `Sandy Creek` save now advances from idle/waiting time into actual locker reevaluation for Ricky Broughton instead of stalling at queue entry forever

### Logging / artifact visibility
- The post-fix live probe landed explicit locker orchestration evidence in `.userdata/dev/config/debug.log` at `10:12:24.706`:
  - `camp locker: queued Ricky Broughton wake_dirty=false queue_size=1 next_turn=0`
  - `camp locker: servicing Ricky Broughton queue_size=1 now=5227550`
  - `camp locker: plan for Ricky Broughton locker_tiles=1 current_items=12 candidates=0 changed_slots=1 reservations=0`
  - `camp locker: serviced Ricky Broughton applied=true queue_remaining=0 next_turn=5228150`
- Meaning: the queue survived post-load validation, the worker reached real service, the planner ran against the live locker tile, and the runtime recorded the resulting cooldown turn.

### Future Josef play packet
- Keep `python3 tools/openclaw_harness/startup_harness.py start --profile dev --world 'Sandy Creek'` as the baseline launch/load smoke for later locker iterations.
- Josef live-check packet on `dev` / `Sandy Creek`:
  - stand near `Debug Central` long enough for camp downtime to tick
  - confirm only one worker reevaluates at a time
  - confirm obvious upgrades / duplicate cleanup happen without churny gear swapping
  - confirm displaced gear winds up back on the locker tile
  - confirm the behavior is legible enough in actual play, not just in `debug.log`

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
- Fresh full `tests/cata_test` rebuilds on this Mac are reliable again, but the path is annoyingly specific: rebuild `cataclysm.a`, use top-level `make -j4 tests`, and avoid direct `make -C tests cata_test` if you want the exported C++17/build flags to stay intact. Also, `cataclysm-tiles` is a real target here only with `TILES=1`.
- Direct deterministic `show_board` responses may still work live even when no fresh block appears in `config/llm_intent.log` (and sometimes not in the usual debug log either). Treat log absence there as an instrumentation/visibility gap, not as proof that the board-reply path is broken. If later inspection wants reliable artifacts for that path, it needs either an explicit logging hook or a different artifact source.
- Keep manual test packets short and concrete: what changed, what to try, expected result, suspicious edge cases.
- One fresh `make clean-tests && make -j4 tests cataclysm-tiles` attempt on this Mac compiled the current `faction_camp.cpp` slice but then died in the local archive step (`ranlib: can't open file: cataclysm.a`). Treat that as a local build/archiver annoyance to revisit, not as proof that the movement-slice logic itself regressed.
