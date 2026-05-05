# CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0 — 2026-05-05

Status: ACTIVE / SMOKE-OUT ROW GREEN / SIGHTED BANDIT ROW REMAINS

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

2026-05-05 smoke-out checkpoint:
- Implementation/source-path: `src/bandit_live_world.*` and `src/do_turn.cpp` now carry smoke-obscured local-gate input from live `fd_smoke` on the watcher tile or between player/camp and watcher; smoke-obscured camp watchers hold off/probe/stalk cautiously instead of opening shakedown/combat-forward, hot defended-doorstep pickup treats smoke-obscured leads as hot, and sight-avoid candidates prefer smoke-clear adjacent reposition or emit an explicit blocked/no-passable-candidate reason.
- Deterministic/source-path gate: `./tests/cata_test "bandit_live_world*" --reporter compact` -> 43 test cases / 1002 assertions, covering bandit current/recent sight discipline, no-cover fallback, smoke-obscured lead handling, and cannibal outcome split with no shakedown leakage.
- Staged/live smoke row: `.userdata/dev-harness/harness_runs/20260505_101407/`, scenario `bandit.scout_stalker_smoked_watcher_live`, evidence class `feature-path`, `feature_proof=true`, 11/11 step-local rows green. Same-run artifacts show `local_gate ... posture=hold_off ... smoke_obscured=yes ... smoke_sightline=yes ... shakedown=no ... combat_forward=no` and bounded adjacent `bandit_live_world sight_avoid: exposed -> repositioned ... distance=1 ... reason=repositioning because smoke obscures lead`.
- Build/format footing: `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests LINTJSON=0 ASTYLE=0`; `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`; `git diff --check`; scenario/fixture JSON validation.

Claim boundary: this credits source-path/deterministic smoke and cannibal split plus one staged/live bandit smoke-out feature-path row. It does not yet close the separate staged/live currently-sighted bandit watcher row, natural random discovery, full vertical assault, full cannibal raid/contact, or tile-perfect smoke physics.

Useful matrix footing:
- `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md` — shared hostile standoff / sight-avoidance switch;
- same matrix — player counter-signal / smoke-out behavior;
- same matrix — bandit and cannibal staged/live rows.

## Success state

- [ ] Sighted bandit watcher near a defended camp/house chooses a real break-LoS/backoff/reroute/escalate/blocker path and does not continue visible doorway/casing-goblin pickup behavior.
- [x] Bandit smoke-out proof shows smoke on the watcher tile or sightline changes the lead/posture and avoids same-tile smoke camping. _Credited staged/live row: `bandit.scout_stalker_smoked_watcher_live`, run `.userdata/dev-harness/harness_runs/20260505_101407/`, evidence class `feature-path`, 11/11 step-local rows green, `feature_proof=true`._
- [x] Cannibal sight/smoke proof consumes the shared discipline while preserving cannibal outcome split: no shakedown, cautious stalk/withdraw under bad odds, attack/contact only when justified. _Credited source-path/deterministic `bandit_live_world*` gate._
- [x] Deterministic or source-path gates cover current-LoS, recent-LoS, cover/no-cover fallback, and smoke-obscured lead handling. _Credited `bandit_live_world*` -> 43 cases / 1002 assertions._
- [ ] Credited live/staged rows name run ids, actor/profile, distance/threat/LoS/smoke footing, decision/reason traces, warning/debug status, and claim boundary.
- [ ] `Plan.md`, `TODO.md`, `TESTING.md`, `SUCCESS.md`, `andi.handoff.md`, and `doc/work-ledger.md` agree before closure.
