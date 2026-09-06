# Active Phase-3 projection

## Current contract and evidence

The 2026-09-06 refrozen `.de67/DFS.md` and current owner contract in `.de67/WEC.md`
govern this projection and every worker handoff. Fresh testing covers every in-scope CAOL
family, including historically green behavior. Old acceptance remains valid for its original
scope and cannot discharge this campaign. Preserve existing repairs; inspect current source
before assuming a historical receipt still names an open implementation gap.

DE67 loop faults go through mutation. Harness faults are recoverable repository work for the
coordinator, mutator or repair worker. CAOL gameplay observations go on the suspected-bug list
with expected/observed behavior, exact evidence, affected tests and blocking consequences.
Only Josef's explicit promotion authorizes a finding, then a mutator DFS update and fix/retest
plan. Classify by responsibility, not source filename. Continue independent tests while an
owner decision is pending; retain blocked tests. All remaining tests blocked means awaiting
the owner, never complete. Runtime or memory must not automatically terminate games; preserve
native quit/finish/cleanup. Preparation is zero-credit and every result has its own verdict.

The previous full projection is preserved at Git `38ff17ef99:.de67/work-ledger.md` and
`.de67/state/review-owner-dced6f928643/before-work-ledger.md`. SQLite acceptances, receipts,
retired deadlines and full artifacts remain unchanged. Earlier tactics are historical evidence.

## Accepted footing

Historical R-SURFACE-001 through R-SURFACE-010, R-027, R-028 and R-030 remain accepted
at their original scope. Their full status projections and exact durable receipts live in the DFS;
this active ledger summarizes footing without replacing those records. R-SURFACE-011, R-033 and
R-036 retain the corresponding fresh obligations. Qualification is available; no old tooling
prerequisite blocks the new campaign.

## Owner correction: speech needs game turns (2026-09-06)

Josef's explicit clarification: CDDA is turn based. After submitting NPC speech or another LLM
request, wait until the LLM calculation has actually finished, then pass a game turn so the game
can apply the result. If further behavior needs simulation time, pass further turns and observe.
A fixed shell sleep does not prove calculation completion, and neither sleep nor `look` advances
game turns. Do not diagnose missing NPC behavior before completing both parts of this sequence.

Live inspection of R-026-exploration-004 confirmed that text run
`2e0d11359dac2663ca3a03dae6cbe0e4927119a6f73476c3459d9607e36034c9` submitted
"Katharina, please report your current camp task.", ran shell `sleep 8`, observed the same World
frame 10, and finished without a subsequent turn-advancing action. Both `world.pause` and
`world.wait` were advertised. Preserve its inconclusive witness; it does not establish broken
NPC attribution or failed intent dispatch.

Carry this correction into the active worker brief now. Repeat the speech route, observe actual
LLM completion through the available request/status evidence, then explicitly pass a turn using
the current semantic World action and inspect the recipient, reply, applied result and behavior.
Verify actual game-time/turn advancement. Respect the current input owner if a menu or prompt
intervenes. Missing completion observability must stay explicit; an arbitrary sleep cannot replace
it. If request launch itself requires a simulation step, establish that from the live route and
advance it as needed rather than waiting forever for work that has not started. A rerun supplying
these missing steps is a corrected test, not forbidden replay of the earlier attempt.

Assess incorporating this completion-then-game-turn sequence into the existing speech/playtest
macro through its assigned harness owner. Preserve request completion evidence, native action
receipts and post-turn observations, and verify the real interaction if implemented. This is
harness/workflow correction, not authorization for a CAOL gameplay fix. Preserve this owner
instruction when rewriting the current handoff until the corrected route has been verified.

## Current campaign

- [ ] R-SURFACE-011 — Current input-owner coverage has drifted beyond the accepted checkpoint.
  - DFS slices: `R-SURFACE-011-S001`
  - Current handoff: Audit current required native input owners and close coverage drift through harness repair and renderer proof. Keep gameplay observations under the owner promotion boundary.

- [ ] R-026 — No current-source integrated CAOL feature package yet binds the living-base,
  bandit, cannibal, signal-control, and flesh-raptor families through one audited established-base
  footing with independent mechanical, causality, feel, persistence, and cleanup evidence plus a
  usable package guide.
  - DFS slices: `R-026-S001`
  - Current handoff: Durable receipt `503a754d3b886511b1c323486c3e53f011bc8f25bfcbbd621b07df57bdb9d626` (`.de67/state/r026-exploration-006-worker-receipt.json`) preserves two fresh narrow R-031 results: Katharina followed and moved during native turns in run `b442104b488ef85be85c3c11e720212e84e85d8b5c06d14fe866f298d31d8274`; an independent camp-return order produced CAMP_RESIDENT assignment in run `f593f0b63ce9c4cfae2076257319023b3061d649c0e6bf788f8dbce14b64e62f`. R-026-exploration-006 is terminal, not accepted. Continue with a fresh independent LLM intent/context route correlating actor, request/context, reply and resulting action, or independent sibling campaign work. Preserve both prior harness repairs and their recovery receipts; neither these repairs nor the two native-order results discharge full R-031 or R-026. Both runs have finish witnesses and observed process exits; bridge status reports reentry_failed and no native-exit credit, so native cleanup/reentry proof remains open.
  - Subtasks:
    - [done] preserve-qualified-footing :: Existing package, harness qualification and historical proof remain available at their original ceilings.
    - [open] audit-current-package-footing :: Verify current source, executable, fixtures and preparation limits for the fresh campaign.
    - [open] prove-living-base :: Bind independent fresh R-031 and R-032 results.
    - [open] prove-hostile-and-signal-routes :: Bind independent fresh R-029 and R-033 results.
    - [open] prove-persistence-and-raptors :: Bind independent fresh R-034 and R-035 results.
    - [open] compare-integrated-performance :: Bind fresh R-036 matched workload results.
    - [open] publish-package-guide-and-verdicts :: Preserve reproducible bindings, evidence, feel, blocked tests and independent verdicts; close only when all required outcomes pass.

- [ ] R-029 — Bandit and cannibal camps do not yet have a proved coherent natural
  signal-to-response route with correct night-raid commitment and operation-scoped shakedown,
  combat, and paid-departure ordering.
  - DFS slices: `R-029-S001`
  - Current handoff: R-029-exploration-040 proved the current-source native-exit lifecycle in run `8f718f4c…`: final YES response SHA `25d4fe…` and PID 85832 exit 0. The same attempt found that normal registry reentry repeated the fixture installer and overwrote the saved world. Its repair adds declared `--reentry-command-json` replacement support in `tools/openclaw_harness/cockpit_file_bridge.py` and supplies the same-token `registry-launch --post-relaunch-continuation` command from `scenario_registry_cli.py`; 69 focused tests and `py_compile` pass. These two harness files are modified and must be preserved. Live bridge session `.userdata/openclaw_harness/bridge-sessions/selected-e63dbcc2d487465795e73d812154a654` is ready with binding `5b0660…`, run `d860d76b…`, game PID 87818, current World ownership, and a zero-credit `f_brazier` at relative `[1,1]`; query its exact response with `python3 tools/openclaw_harness/cockpit_file_bridge.py response-status --session-dir .userdata/openclaw_harness/bridge-sessions/selected-e63dbcc2d487465795e73d812154a654 --request-id <current-request-id>`. First continue with lighter UID `14511`, activate, southeast, confirm, north retreat, journal and `run.finish`; then verify declared reentry before independently observing saved fire and the staffed-observer, camp-memory, scout, report, and response ancestry. The placed brazier and fire creation are setup only. Preserve the flawed run's response `84d424…`, journal `bee0eda…`, PID 86376 exit 0, and overwrite ceiling without replay. Preserve completed Pay evidence and earlier receipts at their original ceilings. Keep night raid, refusal or incomplete payment, player attack, Fight, rolling travel, and witness publication open. Gameplay contradictions are suspected bugs only until Josef promotes them.

- [ ] R-031 — Living NPC intent/context, follow/stay travel and camp routing lack fresh package proof.
  - DFS slices: `R-031-S001`
  - Current handoff: Freshly test actual NPC context/reply/action, follow/stay travel, ambient and camp routing with independent outcomes and competing-owner controls.

- [ ] R-032 — Camp establishment, mission completion and Locker/Patrol/Food/Storage behavior lack fresh proof.
  - DFS slices: `R-032-S001`
  - Current handoff: Freshly test native establishment, actual mission completion, Food, Storage, Locker and Patrol behavior. A prepared camp, opened selector or debug transform cannot prove these outcomes.

- [ ] R-033 — Physical signal controls, camp memory and local/overmap boundary behavior need fresh evidence.
  - DFS slices: `R-033-S001`
  - Current handoff: Freshly test physical light/smoke/sound controls, staffed observation and memory, deduplication/aging, and local/overmap boundaries. Preserve older R-027 evidence separately.

- [ ] R-034 — Fresh package changes lack new-process persistence and continued-behavior evidence.
  - DFS slices: `R-034-S001`
  - Current handoff: Independently save and replace the native process after actual package changes; prove durable identity/state and continued behavior without claiming persistence of transient queues.

- [ ] R-035 — Flesh-raptor orbit/swoop/fallback and encounter feel lack fresh package proof.
  - DFS slices: `R-035-S001`
  - Current handoff: Freshly test native flesh-raptor orbit/swoop/fallback and encounter feel with controls. Writhing stalkers and zombie riders remain excluded.

- [ ] R-036 — The fresh combined living-base/hostile-ecology package lacks matched performance evidence.
  - DFS slices: `R-036-S001`
  - Current handoff: Compare fresh matched integrated workloads with independent correctness, performance and feel evidence. Preserve R-028 results without substituting them for this comparison.

## Execution and closure

Choose independent tests from these outcome routes and expose subdivisions when useful. These
claim entries need not be one worker each or one uninterrupted walkthrough. R-SURFACE-011 repairs
only the owners blocking a given route; independent tests continue. A prerequisite that depends on
its own output is a non-credit bootstrap followed by independent validation, not repeated polling.
R-026 collects the independent results; package closure requires every specified outcome, a usable
guide, and honest persistence/cleanup/feel evidence. Gameplay contradictions remain suspected bugs
until Josef promotes them. Preserve evidence and waiting state without manufacturing completion.
