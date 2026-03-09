# C-AOL Release Roadmap

## Objective
Ship tested Windows and Linux releases for:
- `port/cdda-master`
- `port/cdda-0.H`
- `port/cdda-0.I`
- `port/ctlg-master`

## Status
- `Done`: orchestrator replay flow is stable and `rerere` is enabled (`rerere.enabled=true`, `rerere.autoupdate=true`).
- `Done`: orchestrator now has an AOL parity gate and stronger Codex prompts, so branch runs fail if executor-side AOL wiring is still missing even when the branch compiles.
- `Done`: Windows `BCryptGenRandom` linker fix (`-lbcrypt`) now lives on `master` and is included in both delta and patchset replay paths for future port runs.
- `Done`: `port/cdda-0.I` now has source-level AOL executor parity again on branch: queue promotion, `execute_llm_intent_action`, target/item-target handling, timed panic behavior, and targeted `look_around` pickup are present. Remaining work on `0.I` is build/smoke validation and keeping obsolete prototype commits out of future replays.
- `Done`: `tools/porting/patchsets/cdda-0.I-ignore.txt` now captures obsolete-on-target prototype commits so future `port/cdda-0.I` patchset runs skip stale `guard_area` / `follow_player` / `use_gun` replay noise.
- `Done`: `port/cdda-0.H` now has a branch-local Linux PCH race fix in `Makefile`; the PCH is written to a temporary file and moved into place atomically so parallel WSL builds do not consume a half-written `gpcWrite` artifact.
- `Done`: `port/cdda-0.H` now also uses the safer git diff flags in the `version` rule, which suppresses CRLF warning noise and lets `just_build_linux.cmd --unclean > debug.txt 2>&1` exit `0` cleanly under PowerShell.
- `Blocked on testing`: do not switch the orchestrator default `AolSourceRef` from `master` to `port/cdda-master` until `port/cdda-master` passes real in-game AOL smoke tests.
- `In progress`: finish release validation and packaging parity across all `port/*` branches now that `port/cdda-0.H` has source-level AOL executor parity again and passing Windows and Linux builds.

## Urgent: AOL Port Parity

This is still the main release blocker. Static comparison now shows that `port/cdda-master`, `port/ctlg-master`, `port/cdda-0.I`, and `port/cdda-0.H` all have the AOL executor pipeline at source level. The remaining release work is build/smoke validation and keeping branch-specific fixes small rather than re-porting missing executor code.

- `Source of truth`: use `port/cdda-master` as the primary AOL reference while checking `port/ctlg-master` for branch-safe variants.
- `Promotion rule`: keep the orchestrator default AOL source on `master` for now; only promote `port/cdda-master` to default source after it has been tested in-game and confirmed behavior-complete.
- `Done on port/cdda-0.I`: queue promotion, executor/action-target/item-target handling, and targeted `look_around` pickup flow are all present on the branch tip. Remaining work there is validation and replay hygiene, not missing AOL code.
- `Done on port/cdda-0.H`: queue promotion, executor/action-target/item-target handling, timed panic behavior, and targeted `look_around` pickup are present on branch tip. On March 9, 2026, both `just_build.cmd --unclean > debug.txt 2>&1` and `just_build_linux.cmd --unclean > debug.txt 2>&1` completed successfully, and the Linux rerun produced no `error:`, `fatal error`, `undefined reference`, `make: ***`, or `NativeCommandError` matches after the atomic PCH plus safer git diff fixes. Remaining work is in-game smoke coverage.

### Immediate fix order
- [x] Fix `port/cdda-0.I` first. Source-level AOL parity is restored there, so use it as the proof that the missing layer was executor wiring rather than parser-side plumbing.
- [x] Preserve the `0.I` replay learnings in the orchestrator patchset/ignore rules so future reruns do not stall on obsolete `guard_area` / `follow_player` / `use_gun` history.
- [x] Port the same executor path into `port/cdda-0.H`, then adapt only the unavoidable upstream differences.
- [ ] Treat `port/cdda-master` behavior as the acceptance target: same shout loop, same random-call behavior, same timed panic behavior, same `look_around` / `look_inventory` follow-up behavior.

### Files to diff first
- `src/npc.cpp`
  - Compare the AOL queue promotion logic on `port/cdda-master` vs `port/cdda-0.I` and `port/cdda-0.H`.
- `src/npcmove.cpp`
  - Port the missing AOL executor functions and call sites:
  - `execute_llm_intent_action`
  - `apply_llm_intent_target`
  - `apply_llm_intent_item_targets`
  - the turn logic that consumes `state.active`, advances the queue, applies timed panic decay, and performs `look_around` pickup work.
- `src/npc.h`
  - Keep state layout aligned enough for the ported executor code to compile cleanly on each target.

### Concrete parity checklist
- [x] `set_llm_intent_actions` queue is actually consumed during NPC turns.
- [x] `state.active = state.queue.front()` equivalent exists on the older ports at the correct turn boundary.
- [x] `execute_llm_intent_action( state.active )` equivalent is called from NPC turn/move processing.
- [x] `panic_on` forces temporary panic/run-away behavior for about 20 turns.
- [x] `panic_off` performs the gradual calm-down behavior for about 30 turns.
- [x] `apply_llm_intent_target()` runs so `attack=<target>` guidance is not parser-only.
- [x] `apply_llm_intent_item_targets()` runs so `look_around` actually drives item pickup instead of stopping at selection.
- [x] `look_inventory` follow-up requests still execute inventory wear/wield/activate/drop actions branch-safely.
- [ ] Random calls still work after the executor port and do not regress ordinary NPC AI when AOL is disabled.

### Fast verification loop
- [x] On `port/cdda-0.H`, compile the changed AOL files first:
  - Windows/MSYS2:
    - `make -j8 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=0 objwin/tiles/llm_intent.o objwin/tiles/npc.o objwin/tiles/npcmove.o`
- [ ] On each target, compile the changed AOL files first:
  - Linux/WSL:
    - `make -j8 TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 obj/tiles/llm_intent.o obj/tiles/npc.o obj/tiles/npcmove.o`
- [x] Run full Windows validation on `port/cdda-0.H`:
  - `just_build.cmd --unclean > debug.txt 2>&1`
- [x] Then run full validation:
  - `just_build_linux.cmd --unclean > build_logs/<target>-linux.log 2>&1`
- [ ] In-game smoke test minimum:
  - follower NPC hears `C` + `b`
  - response text appears
  - at least one movement/equip action executes
  - `panic_on` and `panic_off` visibly behave
  - `look_around` picks something up when valid items are nearby

### Anti-drift rule
- Keep branch-local changes as small as possible.
- If `port/cdda-0.I` or `port/cdda-0.H` needs a special-case compile fix, isolate it to that branch and document it in the commit message or here.
- Do not accept parser-only parity as done. AOL is only ported when the executor path works in play, not just when `src/llm_intent.cpp` matches.

## Execution Roadmap

### 1) Port debug sweep (post-orchestrator)
- Treat orchestrator replay as mostly complete; only re-run from `master` when patch queue refresh is needed.
- Use redirected build logs to isolate the first hard error per target and fix branch-specific fallout.
- Keep per-target fixups branch-local only when unavoidable, and document intentional divergence.

### 2) Build matrix (logs redirected)
- For each target branch run:
  - `just_build.cmd --unclean > build_logs/<target>-win.log 2>&1`
  - `just_build_linux.cmd --unclean > build_logs/<target>-linux.log 2>&1`
- Use the helper defaults for matrix speed:
  - Linux dependency bootstrap is opt-in (`--install-deps`).
  - Background summary generation is opt-in (`--with-summary`).
  - Linux build-validation runs disable `ASTYLE` checks.

### 3) Smoke tests
- For each target branch run:
  - `build_and_run.cmd --unclean`
  - `build_and_run_linux.cmd --unclean`
- Validate at least:
  - Game reaches main menu.
  - Save/world load works.
  - NPC shout/speech loop (`C` + `b`) still behaves correctly.

### 4) Packaging audit
- Required in release artifacts for every target:
  - `tools/llm_runner/**`
  - `data/json/npcs/Backgrounds/Summaries_short/**`
  - `README.md`
  - `Plan.md`
  - `TechnicalTome.md`
  - `Agents.md`
- Ensure CAOL branding is present in README/title assets, with a short compatibility disclaimer on port builds.

### 5) GitHub release
- Produce 8 artifacts total (4 targets x 2 platforms).
- Publish with commit hashes and short per-target notes.
- Include known limitations for any target with behavior diffs.

## To-Do Later (Fun Big Bets, Compute-Lean)

### Program constraints
- Keep inference sparse: high mechanical agency from deterministic simulation, with LLM calls only at decision checkpoints.
- Keep Linux/Windows parity for all tooling and tests.
- Keep fail-safe behavior deterministic: if parsing fails, fallback to non-LLM defaults.
- Use context-gated triggers only (NPC and faction): no blind periodic spam.

### Codebase anchors (current integration points)
- LLM trigger path:
  - `src/npctalk.cpp` (`llm_intent::enqueue_requests` on speech).
  - `src/do_turn.cpp` (`llm_intent::enqueue_random_requests` per-turn check).
- LLM request/runner/parser path:
  - `src/llm_intent.cpp` (`build_snapshot_json`, `handle_request`, `extract_lenient_csv`, `process_responses`).
  - `tools/llm_runner/runner.py` (async runner process and self-test).
- NPC action execution path:
  - `src/npc.cpp` (`set_llm_intent_actions`, random-call scheduling helpers).
  - `src/npcmove.cpp` (`execute_llm_intent_action`).
- Overmap movement/off-bubble path:
  - `src/npcmove.cpp` (`set_omt_destination`, `go_to_omt_destination`, `reach_omt_destination`).
  - `src/overmapbuffer.cpp` (`get_travel_path`, `move_hordes`, `move_nemesis`, `process_mongroups` dispatch).
  - `src/do_turn.cpp` (periodic overmap processing entry points).
- Persistence path:
  - `src/savegame.cpp` (`serialize_dimension_data`, `unserialize_dimension_data` with overmap global state).
- Test harness path:
  - `tests/CMakeLists.txt` (auto-includes `tests/*.cpp` in test binaries).
  - `tests/test_main.cpp` (`[nogame]`/world-init behavior and `test_data` loading).
  - Existing patterns in `tests/overmap_test.cpp` and `tests/simple_pathfinding_test.cpp`.

### 1) Bandit overmap director (LLM planner, low frequency)
- `Status`: [ ] prompt contract locked [ ] parser/tests [ ] scheduler wiring [ ] world integration [ ] tuning
- `Goal`: bandit groups feel alive on the overmap without constant inference.
- `Primary files`:
  - `src/llm_intent.cpp` for request queue + strict parser implementation.
  - `tools/llm_runner/runner.py` for compact planner prompt mode.
  - `src/options.cpp` for cadence/budget tuning options (if exposed to players).
- `Snapshot scope`:
  - 8x8 overmap-tile local view around each bandit group.
  - Compressed threat/resource signals (noise, hordes, settlements, loot opportunities).
  - Group state (size, morale, ammo/food pressure, mission/stance).
- `Control granularity`:
  - Preferred: 1 call controls a micro-squad of 2-3 bandits.
  - Optional: one "leader" call controls many units only through high-level doctrine/objective shifts.
  - Avoid per-bandit calls unless in high-stakes local combat.
- `Prompt contract v1` (short in/out):
  - Input: compact world snapshot + objective hints.
  - Output: exactly one command:
    - `Stay`
    - `Move <dir>`
    - `Move <dir> <dir>`
    - `Move <dir> <dir> <dir>`
  - Allowed directions: `N NE E SE S SW W NW`.
  - Invalid output auto-coerces to `Stay`.
- `Cadence`:
  - Far from player: one planner call every 30-90 in-game minutes (jittered).
  - Near activity hotspots: one planner call every 10-20 in-game minutes.
  - Hard cap per faction/group to prevent call storms.

### 1.5) Hierarchical control (faction -> squad -> actor)
- `Status`: [ ] schema [ ] deterministic decomposition [ ] tests [ ] tuning
- Faction-level LLM call:
  - Rare call that sets doctrine + regional objective (`raid east`, `evade city`, `fortify hub`).
- Squad-level LLM call:
  - More common than faction calls; assigns short route intent for a 2-3 NPC squad.
- Actor-level execution:
  - Deterministic AI handles concrete movement/combat/inventory decisions.
  - LLM is escalated only when context gate says situation is ambiguous/high impact.
- Result:
  - One call can influence many actors safely via deterministic decomposition.

### 2) Overmap movement execution layer (deterministic motor system)
- `Status`: [ ] movement queue schema [ ] route execution [ ] stuck recovery [ ] tests
- Add a per-group `llm_route_queue` and `llm_goal_omt`.
- `Primary files`:
  - `src/npcmove.cpp` for destination + route execution behavior.
  - `src/overmapbuffer.cpp` / `src/simple_pathfinding.cpp` for overmap path construction.
  - `src/coordinates.h` / `src/coordinates.cpp` for safe abs/map/omt conversions.
- Planner proposes direction steps; engine converts to concrete overmap targets.
- Existing AI/pathing remains the mover; LLM only sets destination intent.
- If route is blocked or stale:
  - Repath once deterministically.
  - If still blocked, clear queue and fallback to local AI behavior.
- Keep this layer fully functional without any LLM calls.

### 3) Player-map tactical control (reality bubble, event-driven)
- `Status`: [ ] trigger policy [ ] snapshot delta format [ ] action budget [ ] regression tests
- Reuse current follower-style snapshot/action pipeline with stricter event gates.
- `Primary files`:
  - `src/npctalk.cpp` (direct speech trigger entry).
  - `src/do_turn.cpp` and `src/npc.cpp` (random/event cadence scheduling).
  - `src/llm_intent.cpp` + `src/npcmove.cpp` (parse + execute actions).
- Trigger on events: contact, severe damage, low resources, direct order, tactical deadlock.
- Add periodic random check-ins only with per-NPC jittered cooldown.
- Keep response/action parsing strict and short; fallback to normal NPC AI on parse fail/timeout.
- `Context-gated trigger policy (v1)`:
  - Trigger if threat delta rises sharply, squad cohesion breaks, objective changes, or player issues complex intent.
  - Skip if recent deterministic policy still valid and state delta is below threshold.
  - Escalate call priority for panic, hostage/ally-down, chokepoint fights, or friendly-fire risk.

### 4) Off-bubble life simulation (slow heartbeat)
- `Status`: [ ] faction state model [ ] heartbeat scheduler [ ] serialization [ ] perf tests
- Simulate distant groups with lightweight state transitions:
  - movement, attrition, scavenging, conflict, territory pressure.
- `Primary files`:
  - `src/do_turn.cpp` for coarse periodic execution cadence.
  - `src/overmapbuffer.cpp` + `src/overmap.cpp` for map-level batch updates.
  - `src/savegame.cpp` for persisted timers/state across save/load.
- Use batched updates on coarse ticks (e.g., 30-60 in-game minutes).
- Promote to higher-fidelity simulation only when entering player influence radius.

### 5) NPC conversation stream + toolcalls
- `Status`: [ ] schema draft [ ] parser [ ] command adapters [ ] safety rails [ ] UX pass
- Move toward free-text dialogue plus strict toolcalls for:
  - `trade`, `quest`, `camp_order`, `follow_order`, `info_request`.
- Keep conversation semantics deterministic:
  - Toolcall whitelist only.
  - Argument validation.
  - No side effects on malformed calls.

### 6) OpenClaw plugin (debug-first, not full autonomy yet)
- `Status`: [ ] adapter prototype [ ] replay harness [ ] eval metrics [ ] CI smoke tests
- Purpose: use an agent loop to regression-test mechanics and catch gameplay breakage.
- `Current repo reality`: no OpenClaw implementation exists yet; integrate as a separate adapter first.
- `Primary files to reuse`:
  - `src/llm_intent.cpp` runner bridge and existing request/response logging.
  - `tools/llm_runner/runner.py` for IPC model execution and trace behavior.
  - `config/llm_intent.log` + `config/llm_intent_runner.log` as initial replay/debug artifacts.
- `Plugin shape`:
  - Observation adapter: compact ASCII/frame features + key game counters.
  - Action adapter: constrained key command set with cooldown/rate limits.
  - Runner: step loop with full trace logs (`obs`, `action`, `reward`, `outcome`).
- Start with scripted/debug scenarios before open-ended play.
- Keep cargo/state handling shallow initially; focus on reproducible bug-finding runs.

### 7) Pokemon-style lessons to reuse (for OpenClaw)
- Most successful setups used:
  - emulator instrumentation or reliable screen-state extraction,
  - tiny discrete action spaces,
  - frame/step skipping and macro-actions,
  - reward shaping around clear milestones,
  - heavy replay logging for deterministic regression.
- Apply same principle here: small action grammar + strict harness + replayable tests first.

### 8) TDD and validation strategy (required)
- `Status`: [ ] fixtures [ ] unit tests [ ] integration tests [ ] perf budget tests [ ] nightly replay
- Unit tests:
  - overmap output parser (`Stay`/`Move ...`) with fuzzed malformed outputs,
  - route queue behavior and fallback states,
  - scheduler throttling and cooldown limits.
- `Target test files`:
  - `tests/llm_overmap_parser_test.cpp`
  - `tests/llm_overmap_scheduler_test.cpp`
  - `tests/llm_overmap_movement_test.cpp`
  - `tests/llm_offbubble_sim_test.cpp`
- Integration tests:
  - fake planner responses drive deterministic overmap movement,
  - off-bubble heartbeat updates remain stable across save/load.
- Regression tests:
  - OpenClaw replay scenarios verify no movement/toolcall regressions.
- Performance guardrails:
  - enforce max planner calls per in-game hour,
  - enforce per-tick CPU budget for off-bubble simulation.

### 9) Compute and cost guardrails
- LLM acts as strategic planner, not per-turn controller.
- Use local heuristics for routine behavior between planner updates.
- Cache recent decisions when world state is unchanged.
- Start with debug-only inference budgets, then expand only after profiling.
- Target outcome: high agency feel with low call count and bounded runtime.

### 10) Delivery order (PR slices, smallest safe increments)
- `Status`: [ ] PR-A [ ] PR-B [ ] PR-C [ ] PR-D [ ] PR-E [ ] PR-F
- `PR-A` (foundation, no behavior change):
  - Add strict overmap planner parser + tests for `Stay` and `Move <dir...>`.
  - No world simulation changes yet; parser library only.
- `PR-B` (deterministic movement substrate):
  - Add route queue/state and deterministic execution with fallback.
  - Drive with scripted/fake commands; no LLM calls required.
- `PR-C` (LLM overmap planner integration):
  - Add compact prompt mode and scheduler with hard cooldown caps + context gates.
  - Wire parsed output into route queue.
- `PR-C2` (hierarchical batching):
  - Add faction->squad->actor control path and 2-3 NPC micro-squad batching.
  - Ensure one malformed call cannot break whole-faction behavior (local fallback only).
- `PR-D` (off-bubble heartbeat):
  - Add low-frequency group simulation and save/load persistence.
  - Add profiling counters and budget asserts.
- `PR-E` (player-map event triggers):
  - Add event-triggered snapshots and strict action budget gates.
  - Keep current follower behavior as fallback path.
- `PR-F` (OpenClaw debug harness v0):
  - Add observation/action adapter with replay logs and scripted scenarios.
  - No full autonomy target in v0.

### 11) Acceptance gates (must pass before next phase)
- `Parser gate`:
  - 100 percent pass on malformed output corpus.
  - Any invalid output resolves to `Stay` with no crash/no side effects.
- `Movement gate`:
  - Deterministic replay for same seed + same planner commands.
  - Stuck route recovery completes in bounded retries then safe fallback.
- `Simulation gate`:
  - Save/load roundtrip preserves scheduler timestamps and route state.
  - Off-bubble updates remain stable across long-run time skips.
- `Perf gate`:
  - Planner call rate stays under configured ceiling in stress tests.
  - Off-bubble step stays under per-tick CPU budget in profiling scenarios.
- `Gameplay gate`:
  - No regression in current follower shout pipeline.
  - NPCs remain functional when LLM is disabled/unavailable.

### 12) Budget targets (v1, tune after profiling)
- Overmap planner output length:
  - Max 4 tokens (`Stay` or `Move` plus up to 3 directions).
- Overmap planner cadence:
  - Global cap: at most 1 planner request every 2 real-time seconds.
  - Per-group cap: minimum 10 in-game minutes between requests.
- Batch size targets:
  - Squad call size default: 2-3 NPCs.
  - Max directly controlled by one tactical call: 4 NPCs (above that, use doctrine/objective only).
- Player-map tactical cadence:
  - Event-driven first; random check-ins disabled by default for non-followers.
  - Per-NPC tactical calls hard-capped per in-game hour.
- Off-bubble simulation cadence:
  - Coarse tick only (30-60 in-game minutes), batched updates.
  - Immediate promotion to high-fidelity only near player influence.

### 13) Observability and kill switches
- Add counters/logging for:
  - requests attempted, requests accepted, parse failures, fallback activations, cooldown skips.
- Add batching diagnostics:
  - NPCs per call, doctrine changes per day, deterministic override count.
- Add feature flags/options for:
  - overmap planner on/off,
  - hierarchical faction control on/off,
  - off-bubble heartbeat on/off,
  - OpenClaw harness on/off.
- Rollback policy:
  - Any instability defaults to deterministic AI path only, with planner disabled.

### 14) Risk register (with mitigation)
- `Risk`: call storms from many groups.
  - `Mitigation`: global token bucket + per-group cooldown + jitter.
- `Risk`: path thrashing from conflicting planner outputs.
  - `Mitigation`: route stickiness window and minimum commitment horizon.
- `Risk`: save/load divergence for queued movement.
  - `Mitigation`: explicit serialization tests and versioned state schema.
- `Risk`: OpenClaw harness becomes expensive/noisy.
  - `Mitigation`: scripted debug scenarios first, strict step budget, replay-only CI smoke.
- `Risk`: behavior regressions in existing follower loop.
  - `Mitigation`: maintain old pipeline as default fallback and guard with feature flags.

### 15) Extra fun additions (high concept, low compute)
- `Status`: [ ] doctrine system [ ] heat map [ ] signal intercepts [ ] ghost replay [ ] faction wars
- `Bandit doctrine deck`:
  - Each group has a doctrine (`ambush`, `extort`, `scavenge`, `raid`, `evade`).
  - LLM picks/changes doctrine rarely; deterministic AI executes doctrine continuously.
  - Result: strong personality with very few planner calls.
- `Regional heat + notoriety map`:
  - Track player footprint (noise, kills, theft, aid) per overmap region.
  - Bandits react to heat deterministically: hunt high-value zones or avoid dangerous ones.
  - Slow decay keeps long-term consequences without heavy simulation cost.
- `Signal intercept gameplay`:
  - Off-bubble factions emit compact "radio events" from simulation state.
  - Player tools/NPC skills can intercept hints (ambush ahead, convoy sighted, camp weak).
  - Cheap to compute and creates emergent missions.
- `Intent ghost replay debugger`:
  - Persist planner outputs + route execution snapshots and replay them deterministically.
  - Use for fast bug triage and OpenClaw regression checks.
  - Makes debugging satisfying and removes guesswork.
- `Off-bubble faction skirmishes`:
  - Rival bandit groups resolve clashes via lightweight stochastic resolver on heartbeat ticks.
  - Winning/losing updates morale, supplies, territory pressure, and future doctrine choices.
  - World feels alive even without direct player presence.
- `Faction takeover gameplay`:
  - Factions can slowly seize neighborhoods/roads based on doctrine, supply, and pressure.
  - Player/NPC actions can disrupt or accelerate takeover chains.
  - High strategic agency with minimal LLM calls when combined with deterministic heartbeat logic.
- `Command wheel + LLM escalation`:
  - Add deterministic quick orders for common tactics; call LLM only for ambiguous/complex requests.
  - Keeps controls snappy while preserving expressive moments when needed.
  - Directly reduces token/runtime cost during combat.

## Definition Of Done
- All four `port/*` branches complete Windows and Linux build validation.
- Smoke tests are completed on both platforms.
- Packaging contents are parity-checked across targets.
- GitHub release artifacts are uploaded with clear branch/commit provenance.
