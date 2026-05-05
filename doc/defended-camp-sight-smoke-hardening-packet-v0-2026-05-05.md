# CAOL-DEFENDED-CAMP-SIGHT-SMOKE-HARDENING-v0 — 2026-05-05

Status: ACTIVE / GREENLIT / DEFERRED DEBUG-STACK HARDENING

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

Useful matrix footing:
- `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md` — shared hostile standoff / sight-avoidance switch;
- same matrix — player counter-signal / smoke-out behavior;
- same matrix — bandit and cannibal staged/live rows.

## Success state

- [ ] Sighted bandit watcher near a defended camp/house chooses a real break-LoS/backoff/reroute/escalate/blocker path and does not continue visible doorway/casing-goblin pickup behavior.
- [ ] Bandit smoke-out proof shows smoke on the watcher tile or sightline changes the lead/posture and avoids same-tile smoke camping.
- [ ] Cannibal sight/smoke proof consumes the shared discipline while preserving cannibal outcome split: no shakedown, cautious stalk/withdraw under bad odds, attack/contact only when justified.
- [ ] Deterministic or source-path gates cover current-LoS, recent-LoS, cover/no-cover fallback, and smoke-obscured lead handling.
- [ ] Credited live/staged rows name run ids, actor/profile, distance/threat/LoS/smoke footing, decision/reason traces, warning/debug status, and claim boundary.
- [ ] `Plan.md`, `TODO.md`, `TESTING.md`, `SUCCESS.md`, `andi.handoff.md`, and `doc/work-ledger.md` agree before closure.
