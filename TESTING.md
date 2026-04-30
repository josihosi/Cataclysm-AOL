# TESTING

Current validation policy and current evidence only.

This file is not a trophy wall.
Remove stale or completed fluff instead of stacking crossed-off test history forever.

## Validation policy

Use the **smallest evidence that honestly matches the change**.

- **Docs-only change** -> no compile
- **Small local code change** -> narrow build and the narrowest relevant test target
- **Broad or risky code change** -> broader rebuild and relevant filtered tests
- **Before a Josef handoff** or after suspicious stale-binary behavior -> rebuild the real targets and rerun the real smoke path

### Agent vs Josef

Schani should do agent-side playtesting first whenever the harness or direct in-game probing can answer the question.
Josef should be asked only for:
- product judgment
- tone/feel
- human-priority choices
- genuinely human-only interaction

Josef being unavailable is **not** a blocker by itself.
Josef self-testing is **not** a plan blocker and does not belong in the active success state as a gate.
If Josef-specific checks are useful, write them down as non-blocking notes so he can run them later from his own list.
If another good agent-side lane exists, keep moving there.
If several human-only judgments are likely soon, batch them instead of sending tiny separate asks.

### Anti-churn rule

Do not keep rerunning the same startup or test packet when it is no longer the missing evidence class.
If startup/load is already green, and the missing proof is live behavior, then the next probe must target live behavior.
If a target is merely waiting on Josef, do not keep revalidating it unless the code changed.

Current debug-stack attempt rule for the same item/blocker:
- attempts 1-2 may be Andi solo retries, including multiple focused harness attempts in one cron run when each attempt changes setup, instrumentation, or evidence class
- before attempt 3, consult Frau Knackal
- attempts 3-4 are the final changed retries after consultation
- after attempt 4, if code is implemented but proof still fails, add a concise implemented-but-unproven packet to Josef's playtest package and move to the next greenlit debug note; do not close it and do not park it as dead

### Test-to-game wiring rule

A test is not allowed to impersonate implementation. Before claiming gameplay behavior, identify the live code path that consumes the tested seam and name the evidence class that proves it: unit/evaluator, playback/proof packet, live source hook, harness/startup, screen, save inspection, or artifact/log. Deterministic-only packets may close only as deterministic-only packets; if the contract says the game does something, the proof must reach the game path or the claim stays open.

### Bandit overmap-proof rule

For the remaining bandit AI proof packets, single-turn deterministic checks are **not** enough by themselves.
The honest bar now includes real overmap-side multi-turn scenario proof, up to `500` turns where needed, with explicit per-scenario goals and tuning metrics.


## Current validation targets

### Active validation target - CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0

`CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` is the active challenge playtest packet. Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`; imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`.

Validation burden:
- Challenge A: multi-camp structural stress with at least two camps, bounded time passage, before/after site/group state, target reasons, no-repeat state, dogpile/spread/hold behavior, and metrics.
- Challenge B: mixed signal coexistence combining structural bounty with at least one live smoke/fire/light/roof-fire signal, with candidate priority/reason evidence and state-preservation checks.
- Challenge C: reload/resume continuity for a meaningful active outing if practical; otherwise exact deferred seam.
- Do not use setup-only camp/signal presence as proof. The live maintenance/outing path must run and produce before/after evidence.
- Report red/yellow failure honestly if dogpile, stale state, reload loss, CPU churn, or log spam appears.

### Closed validation receipt - CAOL-ROOF-HORDE-NICE-FIRE-v0

`CAOL-ROOF-HORDE-NICE-FIRE-v0` is closed/checkpointed green for v0. Contract: `doc/roof-fire-horde-nice-roof-fire-playtest-packet-v0-2026-04-30.md`; imagination source: `doc/roof-fire-horde-nice-roof-fire-imagination-source-of-truth-2026-04-30.md`; closure proof: `doc/roof-fire-horde-nice-roof-fire-proof-v0-2026-04-30.md`.

Credited run: `.userdata/dev-harness/harness_runs/20260430_191556/` using scenario `bandit.roof_fire_horde_nice_roof_fire_mcw` and fixture `roof_fire_horde_split_wait_from_player_fire_v0_2026-04-29`. Source roof-fire footing remains `.userdata/dev-harness/harness_runs/20260429_172847/`, with prior split proof `.userdata/dev-harness/harness_runs/20260429_180239/`.

Credited evidence:
- Saved roof/elevated fire before/after wait: `t_tile_flat_roof` + `f_brazier` + `fd_fire`, no new fixture `fd_fire` transform, tied to the source normal player-action roof-fire chain.
- Horde before wait: `mon_zombie` at offset `[0,-120,0]`, destination self, `tracking_intensity=0`, `last_processed=0`, `moves=0`; this is footing only.
- Bounded wait: `5m`, turn delta `300` (`5266942` -> `5267242`), wait ledger green.
- Same-run live roof-fire horde signal: `bandit_live_world horde light signal ... source_omt=(140,41,1) horde_signal_power=20 ... elevated_exposure_extended=yes`.
- Horde response after wait: destination retargeted to `[3360,984,1]`, `last_processed=5267242`, `moves=8400`, status `required_state_present`.
- Cost/stability: harness wall-clock `2:34.72`, `14/14` step rows green, `1/1` wait rows green, no runtime warnings/abort; horde-specific timing `not instrumented`.

Preserved caveats: no positive `tracking_intensity` claim, no horde-specific micro-timing, no broader combat/pathfinding or natural multi-day horde discovery claim. Do not rerun this proof as ritual unless Schani/Josef explicitly promote a stricter follow-up.

### Closed validation receipt - CAOL-WRITHING-STALKER-v0

`CAOL-WRITHING-STALKER-v0` is closed/checkpointed green for v0. Receipts: `doc/work-ledger.md`, `SUCCESS.md`, raw intake `doc/writhing-stalker-raw-intake-2026-04-30.md`, imagination source `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`, contract `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`, testing ladder `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`, and mixed-hostile metrics packet `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`.

Credited evidence:
- Monster/stat/spawn footing and rare singleton `GROUP_ZOMBIE` entry validate with JSON checks, focused tests, and `./tests/cata_test "[writhing_stalker]"`.
- Deterministic substrate in `src/writhing_stalker_ai.*` covers interest, latch decay/leash, cover/darkness approach, opportunity, withdrawal, and cooldown behavior.
- Live monster-plan seam in `src/monmove.cpp` routes `mon_writhing_stalker` through local-evidence-gated shadow/strike/withdraw planning without new persisted latch state.
- Support footing: `writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/` and `writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/` prove saved active-monster footing, target acquisition, and the live `live_plan` seam. These support runs do not by themselves close behavior.
- Behavior proofs: `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/`; `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/`; `writhing_stalker.live_no_omniscient_beeline_mcw` -> `.userdata/dev-harness/harness_runs/20260430_173555/`.
- Mixed-hostile metrics/tuning proof: `performance.mixed_hostile_stalker_horde_mcw` -> `.userdata/dev-harness/harness_runs/20260430_181748/`; the run proves active bandit and cannibal stalk jobs, one `mon_writhing_stalker`, and one nearby `mon_zombie` horde, then completes `500` sampled turns plus bounded `30m` wait. Metrics include avg `236.239ms/turn`, hostile cadence `total_us` max `3777`, stalker `eval_us` max `54`, no crash/stderr/debug-error flood, and the tuning readout.

Preserved caveat: Frau accepted the mixed-hostile horde-attribution caveat for v0 closure. Horde presence/setup is proven in `20260430_181748`, but direct horde movement/retarget cost is not instrumented and remains future-only unless Schani/Josef explicitly promotes stricter horde attribution. Do not rerun the same mixed-hostile soup as ritual.

### Closed validation receipt - CAOL-BANDIT-STRUCT-BOUNTY-v0

`CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green for v0. Closure readout: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`. Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`; testing ladder: `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`; receipt: `doc/work-ledger.md`.

Credited live run: `.userdata/dev-harness/harness_runs/20260430_115157/`. Credited live claims: idle camp structural forest bounty dispatch without player smoke/light/direct-range bait; same-run candidate economics (`bounty=8`, `known_threat=0`, `confidence=3`, `effective_interest=11`, `decision=scavenge`); stalking-distance keep-open; arrival/harvest; `arrivals=1`; `members_returned=1`; saved harvested/no-repeat state. Non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/` remains red/inconclusive and must not be reused as closure evidence.

Future structural-bounty playtests are greenlit as optional follow-ups, not blockers: live dangerous/turnback Branch A, live reload-resume, two/four-camp wait stress, mixed signal coexistence, and less-blessed natural-ish idle samples.

Recent completed validation targets have been moved to compact receipts in `doc/work-ledger.md` plus their canonical aux/proof docs. Do not re-expand them here unless one becomes the active validation target again.

Current important receipts:
- `CAOL-ROOF-HORDE-NICE-FIRE-v0` — focused nice roof-fire horde feature-path proof.
- `CAOL-SZM-LIVE-LABEL-v0` — Smart Zone live coordinate-label proof.
- `CAOL-REAL-FIRE-SIGNAL-v0` — real player fire -> bandit signal response.
- `CAOL-ROOF-HORDE-SPLIT-v0` — split-run roof-fire horde response.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` and `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — cannibal behavior closure/confidence uplift.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — scoped bandit camp-map risk/reward checkpoint.
- `CAOL-FUEL-RED-MULTIDROP-v0` and `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — non-credit/superseded surfaces that must not be reopened by drift.

## Pending probes

No active same-lane agent-side probe remains open for `CAOL-ROOF-HORDE-NICE-FIRE-v0`. Nice roof-fire horde proof is green/closed for v0 at `.userdata/dev-harness/harness_runs/20260430_191556/`; stricter positive `tracking_intensity`, horde-specific timing, or natural multi-day horde discovery remain future-only unless Schani/Josef explicitly promote them. `CAOL-WRITHING-STALKER-v0` also remains closed and must not be reopened by drift.

Current green support/behavior probes:
- `writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/` (spawn/save/active-monster footing only).
- `writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/` (target acquisition + `live_plan` seam only).
- `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/` (harness-only noon setup + saved time audit + bright/focused withdrawal live-plan line + save/writeback/active-monster audit).
- `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/` (harness-only vulnerable-midnight setup + saved time audit + shadow-before-strike/strike/cooldown live-plan lines + save/writeback/active-monster audit).
- `writhing_stalker.live_no_omniscient_beeline_mcw` -> `.userdata/dev-harness/harness_runs/20260430_173555/` (harness-only clean noon/no-human-target setup + saved NPC/time/wall/active-monster audits + same-run `target=no`/`sees_player=no` target-probe proof + negative log guards for no target/live-plan/strike/shadow/cooldown + save/writeback/active-monster audit).

Initial greenlit behavior/perf probes are now accounted for:
- `performance.mixed_hostile_stalker_horde_mcw` -> `.userdata/dev-harness/harness_runs/20260430_181748/` (mixed hostile metrics/tuning report; horde direct timing explicitly `not instrumented`).

Preserve `.userdata/smart-zone-audit-live-20260429e/harness_runs/20260429_225644/` as the green Smart Zone Manager live coordinate-label proof. Preserve `.userdata/dev-harness/harness_runs/20260429_180239/` as the green split-run roof-fire horde detection proof and `.userdata/dev-harness/harness_runs/20260429_172847/` as its source player-created roof-fire writeback proof. Preserve `.userdata/dev-harness/harness_runs/20260429_162100/` as green player-lit source-zone fire -> bandit signal proof. Preserve `.userdata/dev-harness/harness_runs/20260430_115157/` as green structural-bounty v0 proof and `.userdata/dev-harness/harness_runs/20260430_114106/` as red/non-credit structural-bounty postmortem. Do not rerun solved rows as ritual.

Future-only watchlist unless Schani/Josef explicitly promotes it:

- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.
- A stricter roof-horde positive `tracking_intensity` proof remains optional/future-only; current green roof proof is retarget/movement-budget metadata after live roof-fire horde signaling.
- Full Smart Zone process-reload disk persistence can be promoted later if Josef wants that stricter audit; the current green proof covers live create/inspect/close-save/reopen coordinate labels.

Stale runs `.userdata/dev-harness/harness_runs/20260429_093118/`, `.userdata/dev-harness/harness_runs/20260429_093509/`, `.userdata/dev-harness/harness_runs/20260429_095021/`, `.userdata/dev-harness/harness_runs/20260429_122807/`, and `.userdata/dev-harness/harness_runs/20260429_122955/` are retained as postmortem evidence only, not fire proof surfaces.

If a later live probe is promoted:
- build the current runtime first when binary freshness matters;
- use one named scenario/command path;
- extract only decisive report/log fields into context;
- after two same-blocker attempts, consult Frau Knackal before attempt 3; after four unresolved attempts, package implemented-but-unproven state for Josef and move to the next greenlit unblocked target.

---

## Reusable commands

Use these when they are actually the missing evidence, not as ritual.

### Narrow camp deterministic pass after a code slice lands
- `git diff --check`
- `make -j4 obj/basecamp.o tests/faction_camp_test.o tests LINTJSON=0 ASTYLE=0`
- focused `./tests/cata_test` filters for the touched camp/locker/patrol reporting path

### Plan status summary command
- `python3 tools/plan_status_summary.py --self-test`
- `python3 tools/plan_status_summary.py /plan`
- `python3 tools/plan_status_summary.py /plan active`
- `python3 tools/plan_status_summary.py /plan greenlit`
- `python3 tools/plan_status_summary.py /plan parked`

### Fresh tiles rebuild only if a later handoff really needs live proof
- `make -j4 TILES=1 cataclysm-tiles`

## Local build caveat

On this Mac, treat top-level `make -j4 tests` as the reliable path for a fresh `cata_test`.
Avoid treating `make -C tests cata_test` as authoritative here; it has been a repeated source of toolchain/stale-build nonsense.
Also: if you actually need a fresh tiles binary, use `make -j4 TILES=1 cataclysm-tiles`; plain `make cataclysm-tiles` is not an honest rebuild path here.
