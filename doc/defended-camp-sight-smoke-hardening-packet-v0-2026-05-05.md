# CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0 — 2026-05-05

Status: CHECKPOINTED GREEN / AGENT-SIDE PROOF COMPLETE / AWAITING FRAU REVIEW

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Parent checkpoint: `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`.

## Why this exists

The eleven-slice Josef live-debug batch is agent-side green, but Slice 2 deliberately stopped at current-pass `5` OMT hold-off and hot-doorstep pickup blocking. The deeper product smell remains worth one bounded follow-up: hostile watchers should not keep being visible garden gnomes, and smoke should not be transparent paperwork.

This packet promotes exactly that deferred hardening row so Andi has a real active target again instead of rerunning completed proof as comfort work. Na bitte, an actual task instead of polishing the receipt book.

## Scope

Primary actors:
- bandit scouts / hold-off / stalking-mode pressure near defended camps or houses;
- cannibal scout/stalk/attack-pressure profiles where the shared sight/smoke discipline applies.

Required behavior:
- current player or camp-NPC line of sight makes a watcher break LoS, back off, reroute through cover/darkness, escalate/report, or record a concrete blocker;
- recent LoS should bias caution without forcing nonsense instant teleporting;
- a hot defended doorstep should stay hot: casual pickup/collect-bounty behavior must not continue there;
- player smoke on the suspected watcher tile, or between the camp and watcher, changes the lead to obscured/uncertain and causes reposition, wait, probe-around, escalation, or a logged blocker instead of same-tile smoke camping;
- bandits preserve bandit outcomes: scout, report, hold off, toll/shakedown-capable escalation when already justified;
- cannibals preserve cannibal outcomes: no Pay/Fight shakedown, stalk/withdraw under bad odds, attack/contact pressure only when odds/reserve/concealment justify it.

## Non-goals

- no full local roof/stair/vertical assault AI;
- no broad overmap pathfinder rewrite;
- no every-camp suicide dogpile;
- no cannibal diplomacy or shakedown UI;
- no tile-perfect smoke simulation;
- no reopening completed shakedown, light adapter, patrol, sorting, debug-spawn, locker, or Monsterbone proof rows.

## Evidence bar

Use the smallest honest proof that reaches the game path.

Minimum expected evidence:
- focused deterministic/unit or evaluator coverage for LoS and smoke decision seams, including no-cover/blocker fallback;
- at least one staged/live bandit defended-camp row where a currently-sighted watcher backs off/breaks LoS/reroutes/escalates and does not keep hot-doorstep pickup behavior;
- at least one staged/live bandit smoke-out row where smoke changes lead/posture and avoids same-tile smoke camping;
- at least one cannibal staged/live or source-path row proving the same sight/smoke primitive is consumed with cannibal outcome split and no shakedown leakage;
- no stale-binary or runtime-version mismatch for feature-path claims;
- explicit caveat if the proof is staged/live rather than natural random discovery.

## Current evidence receipts

2026-05-05 checkpoint:
- Implementation/source-path: `src/bandit_live_world.*` and `src/do_turn.cpp` carry live `fd_smoke` into local-gate input from the watcher tile or the watcher/player-camp sightline; smoke-obscured camp watchers hold off/probe/stalk cautiously instead of opening shakedown/combat-forward, hot defended-doorstep pickup treats smoke-obscured leads as hot, and sight-avoid candidates prefer bounded adjacent reposition or emit an explicit blocked/no-passable-candidate reason. The sight-avoid reason now reports smoke when `smoke_obscured=yes`, even if the same actor is otherwise visible.
- Deterministic/source-path gate: `./tests/cata_test "bandit_live_world*" --reporter compact` -> 43 test cases / 1002 assertions, covering bandit current/recent sight discipline, hot-doorstep pickup blocking, no-cover fallback, smoke-obscured lead handling, and cannibal outcome split with no shakedown leakage.
- Staged/live currently-sighted bandit row: `.userdata/dev-harness/harness_runs/20260505_102525/`, scenario `bandit.scout_stalker_sight_avoid_live`, evidence class `feature-path`, `feature_proof=true`, 10/10 step-local rows green. Same-run artifacts show `local_gate ... active_job=scout ... posture=hold_off ... local_contact=yes ... current_exposure=yes ... shakedown=no ... combat_forward=no` and two bounded adjacent `bandit_live_world sight_avoid: exposed -> repositioned ... distance=1 ... reason=repositioning because exposed` moves.
- Staged/live smoke row: `.userdata/dev-harness/harness_runs/20260505_102629/`, scenario `bandit.scout_stalker_smoked_watcher_live`, evidence class `feature-path`, `feature_proof=true`, 11/11 step-local rows green. Same-run artifacts show `local_gate ... posture=hold_off ... smoke_obscured=yes ... smoke_on_watcher=yes ... smoke_sightline=yes ... shakedown=no ... combat_forward=no` and bounded adjacent `bandit_live_world sight_avoid: exposed -> repositioned ... distance=1 ... reason=repositioning because smoke obscures lead`.
- Build/format footing: `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "bandit_live_world*" --reporter compact`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `git diff --check`; scenario/fixture JSON validation.

Claim boundary: this closes the bounded agent-side staged/live feature-path packet for current sight and smoke-obscured defended-camp watcher behavior plus source-path cannibal split. It does not claim natural random discovery, full vertical assault, full cannibal raid/contact, or tile-perfect smoke physics.

Useful matrix footing:
- `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md` — shared hostile standoff / sight-avoidance switch;
- same matrix — player counter-signal / smoke-out behavior;
- same matrix — bandit and cannibal staged/live rows.

## Success state

- [x] Sighted bandit watcher near a defended camp/house chooses a real break-LoS/backoff/reroute/escalate/blocker path and does not continue visible doorway/casing-goblin pickup behavior. _Credited staged/live row: `bandit.scout_stalker_sight_avoid_live`, run `.userdata/dev-harness/harness_runs/20260505_102525/`, evidence class `feature-path`, 10/10 step-local rows green, `feature_proof=true`; same-run scout/current-exposure/local-contact artifacts show hold-off/no-shakedown/no-combat plus bounded adjacent `distance=1` reposition. Hot-doorstep pickup blocking remains covered by the deterministic/source-path gate._
- [x] Bandit smoke-out proof shows smoke on the watcher tile or sightline changes the lead/posture and avoids same-tile smoke camping. _Credited staged/live row: `bandit.scout_stalker_smoked_watcher_live`, run `.userdata/dev-harness/harness_runs/20260505_102629/`, evidence class `feature-path`, 11/11 step-local rows green, `feature_proof=true`._
- [x] Cannibal sight/smoke proof consumes the shared discipline while preserving cannibal outcome split: no shakedown, cautious stalk/withdraw under bad odds, attack/contact only when justified. _Credited source-path/deterministic `bandit_live_world*` gate._
- [x] Deterministic or source-path gates cover current-LoS, recent-LoS, cover/no-cover fallback, and smoke-obscured lead handling. _Credited `bandit_live_world*` -> 43 cases / 1002 assertions._
- [x] Credited live/staged rows name run ids, actor/profile, distance/threat/LoS/smoke footing, decision/reason traces, warning/debug status, and claim boundary.
- [x] `Plan.md`, `TODO.md`, `TESTING.md`, `SUCCESS.md`, `andi.handoff.md`, and `doc/work-ledger.md` agree before closure.
