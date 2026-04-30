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

### Active validation target - CAOL-WRITHING-STALKER-v0

Current boundary: **monster/stat/spawn footing, deterministic behavior substrate, first live monster-plan seam wiring, split live spawn/target footing, exposed/focused withdrawal scene proof, and live shadow/strike scene proof are green; no-omniscience and Josef's mixed-hostile metrics playtest are next**. Receipt: `doc/work-ledger.md`. Raw intake: `doc/writhing-stalker-raw-intake-2026-04-30.md`; imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`; contract: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`; testing/playtest ladder: `doc/writhing-stalker-playtest-ladder-v0-2026-04-30.md`; mixed-hostile performance packet: `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`.

Current evidence:
- `mon_writhing_stalker` JSON/stat footing and rare singleton `GROUP_ZOMBIE` spawn entry landed with `tests/writhing_stalker_test.cpp`. Evidence: `git diff --check`; `python3 -m json.tool data/json/monsters/zed_misc.json`; `python3 -m json.tool data/json/monstergroups/zombies.json`; `make -j4 tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"`; focused Python JSON audit for singleton `GROUP_ZOMBIE` entry.
- Deterministic AI-decision substrate landed in `src/writhing_stalker_ai.*` and `tests/writhing_stalker_test.cpp`: interest ranking keeps recent human evidence strongest, exposed night light strong, smoke weak/indirect, terrain/edge cover meaningful, and zombie pressure as opportunity; latch is bounded by evidence age/leash/exposure and requires refreshed evidence; approach prefers cover/edge routes and holds at bright exposure; opportunity scoring strikes only from valid latch plus vulnerability/distraction; withdrawal/cooldown block immediate repeat spam. Evidence: `make -j4 obj/writhing_stalker_ai.o tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"` (88 assertions / 7 test cases).
- First live monster-plan seam landed in `src/monmove.cpp` plus `writhing_stalker::evaluate_live_response()`: `mon_writhing_stalker` now gates live target planning on believable local evidence, chooses shadow destinations near cover/darkness/clutter, shadows before distant strike windows, strikes vulnerability windows by targeting the player, and withdraws/cools off with existing `effect_run`; strike now also applies the same short cooldown so immediate relatch spam is blocked after a hit window. No new persisted latch field was added. Evidence: `git diff --check`; `make -j4 obj/writhing_stalker_ai.o obj/monmove.o tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"` (99 assertions / 8 test cases).
- Split spawn-footing proof is green, but only as creature footing: `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/`; `step_ledger_summary.status=green_step_local_proof`; save/writeback mtime gate green; saved `active_monsters` contains one hostile `mon_writhing_stalker` at player-relative offset `[1,0,0]` (`active_monster_counts={'mon_writhing_stalker': 1}`). This does **not** prove live behavior.
- Split target/plan-turn proof is green as the basic live seam: after `make -j4 TILES=1 cataclysm-tiles`, `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/`; `feature_proof=true`, `verdict=artifacts_matched`, `step_ledger_summary.status=green_step_local_proof`. Decisive artifact lines show `target_probe ... target=yes ... sees_player=yes` and `live_plan ... evidence=yes ... target_focus=yes ... eval_us=...`; saved `active_monsters` still contains one `mon_writhing_stalker`. This proves target acquisition and live seam execution, not exposed-retreat/strike closure.
- Exposed/focused withdrawal live scene proof is green after replacing the failed wait-to-noon ritual with harness-only saved-turn setup: `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/`; fixture `mcwilliams_live_debug_noon_2026-04-30` applies only `game_turn=5227200`; saved pre-spawn audit shows `time_of_day_text=12:00:00`, `observed_time_of_day_delta_seconds=0`; live artifact proves `decision=withdraw route=hold_exposed reason=live_exposed_and_focused_withdraw ... target_bright=no stalker_bright=yes target_focus=yes cooldown=no eval_us=12`; save/writeback mtime and saved `active_monsters` audit are green. Step ledger: 11/11 green. Validation after the probe: `git diff --check`, `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, JSON syntax checks, and `./tests/cata_test "[writhing_stalker]"` (97 assertions / 8 test cases) all pass.
- Shadow/strike live scene proof is green after adding a harness-only vulnerable-midnight fixture and making the live response shadow before distant strike windows: `python3 tools/openclaw_harness/startup_harness.py probe writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/`; fixture `mcwilliams_live_debug_vulnerable_2026-04-30` applies `game_turn=5270400`, 60% HP, low stamina, and torso `bleed`; saved pre-spawn audit shows `time_of_day_text=00:00:00`, `observed_time_of_day_delta_seconds=0`; live artifact proves `decision=shadow route=cover_shadow reason=live_shadowing_before_strike_window ... opportunity=74 ... target_focus=no cooldown=no`, then `decision=strike route=cover_shadow reason=live_vulnerability_window_strike ... distance=3 ... cooldown=no`, then `decision=cooling_off ... reason=live_latch_cooldown_blocks_relatched_pressure ... cooldown=yes`; save/writeback mtime and saved `active_monsters` audit are green. Step ledger: 11/11 green. Validation around the probe: `make -j4 TILES=1 cataclysm-tiles`, `python3 -m py_compile tools/openclaw_harness/startup_harness.py`, JSON syntax checks, and `./tests/cata_test "[writhing_stalker]"` (99 assertions / 8 test cases) all pass.

Greenlit proof ladder:

1. Creature truth: monster JSON/stat/spawn data validates and matches the intended rare singleton first-generation zombie-adjacent predator. **Green for initial footing.**
2. Rarity truth: spawn footing does not create ordinary stalker soup; debug/harness can intentionally create one. **Green for initial footing; intentional debug/harness creation and saved `active_monsters` audit are green as support footing.**
3. Interest truth: deterministic helper/evaluator proves human/player evidence and exposed night light outrank empty terrain, smoke stays weak/indirect, cover/edge terrain matters, and zombie pressure increases opportunity rather than threat. **Green at deterministic seam.**
4. Latch truth: recent believable evidence can create a timed/leashed latch, stale evidence decays, and one clue does not become permanent omniscience. **Green at deterministic seam.**
5. Approach truth: stalker prefers cover/darkness/clutter where available and does not direct-beeline across open exposure unless forced by the actual map. **Green at deterministic seam; live path still open.**
6. Opportunity truth: strike pressure rises when the player is hurt, bleeding, low stamina, noisy, sleeping/resting/crafting, boxed by zombies, or moving through clutter; alert/bright/open player states hold or repel pressure. **Green at deterministic seam; live strike path still open.**
7. Strike truth: combat creates short cut/bleed pressure and then either withdraws or changes state; it does not become a tank duel.
8. Withdrawal/cooldown truth: hurt/exposed/focused stalker backs off, cooldown blocks immediate repeat spam, and re-engagement needs refreshed evidence. **Green at deterministic seam and live exposed/focused withdrawal scene (`20260430_163626`).**
9. Persistence truth: any new latch/cooldown state survives save/load, or the packet explicitly avoids new persisted state. **Green for v0: no new persisted latch/cooldown field; live cooldown uses existing monster effect state.**
10. Live truth: live/harness packets prove a real shadow/strike scene plus no-omniscient-beeline negative control and exposed/focus retreat or an explicit future-only classification. **Basic target acquisition / `live_plan` seam, exposed/focus withdrawal, and shadow/strike are green; no-omniscience remains open.**

Initial proposed live scenarios:

- `writhing_stalker.live_shadow_strike_mcw` — one believable stalk/hold/strike/cooldown scene. **Green:** `.userdata/dev-harness/harness_runs/20260430_170528/`.
- `writhing_stalker.live_no_omniscient_beeline_mcw` — no valid evidence, no instant beeline/attack.
- `writhing_stalker.live_exposed_retreat_mcw` — latch exists, exposure/focus/hurt state causes hold or withdrawal. **Green:** `.userdata/dev-harness/harness_runs/20260430_163626/`.
- `performance.mixed_hostile_stalker_horde_mcw` — Josef-requested metrics playtest with bandit camp, cannibal camp, one writhing stalker, and horde on the map. Report must include setup mix, runtime commit/dirty state, in-game window, harness wall-clock, per-turn min/median/p95/max where available, `bandit_live_world perf` min/median/max and slice maxima, stalker/horde timing or explicit `not instrumented`, log spam/stability, and a playability note.

Do not close from JSON alone. Do not close from deterministic tests pretending to be gameplay. Do not close from a debug spawn directly on top of the player and call it stalking. This is horror, not a spreadsheet mugging.

### Closed validation receipt - CAOL-BANDIT-STRUCT-BOUNTY-v0

`CAOL-BANDIT-STRUCT-BOUNTY-v0` is closed/checkpointed green for v0. Closure readout: `doc/bandit-structural-bounty-phase-7-closure-readout-2026-04-30.md`. Canonical contract: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`; testing ladder: `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`; receipt: `doc/work-ledger.md`.

Credited live run: `.userdata/dev-harness/harness_runs/20260430_115157/`. Credited live claims: idle camp structural forest bounty dispatch without player smoke/light/direct-range bait; same-run candidate economics (`bounty=8`, `known_threat=0`, `confidence=3`, `effective_interest=11`, `decision=scavenge`); stalking-distance keep-open; arrival/harvest; `arrivals=1`; `members_returned=1`; saved harvested/no-repeat state. Non-credit run `.userdata/dev-harness/harness_runs/20260430_114106/` remains red/inconclusive and must not be reused as closure evidence.

Future structural-bounty playtests are greenlit as optional follow-ups, not blockers: live dangerous/turnback Branch A, live reload-resume, two/four-camp wait stress, mixed signal coexistence, and less-blessed natural-ish idle samples.

Recent completed validation targets have been moved to compact receipts in `doc/work-ledger.md` plus their canonical aux/proof docs. Do not re-expand them here unless one becomes the active validation target again.

Current important receipts:
- `CAOL-SZM-LIVE-LABEL-v0` — Smart Zone live coordinate-label proof.
- `CAOL-REAL-FIRE-SIGNAL-v0` — real player fire -> bandit signal response.
- `CAOL-ROOF-HORDE-SPLIT-v0` — split-run roof-fire horde response.
- `CAOL-CANNIBAL-NIGHT-RAID-v0` and `CAOL-CANNIBAL-CONFIDENCE-PUSH-v0` — cannibal behavior closure/confidence uplift.
- `CAOL-BANDIT-CAMP-MAP-RISK-REWARD-v0` — scoped bandit camp-map risk/reward checkpoint.
- `CAOL-FUEL-RED-MULTIDROP-v0` and `CAOL-BANDIT-LIVE-SIGNAL-SITE-BOOTSTRAP-v0` — non-credit/superseded surfaces that must not be reopened by drift.

## Pending probes

Current writhing-stalker next step is live/harness behavior proof. Monster/stat/spawn footing, deterministic decision substrate, first live monster-plan seam wiring, debug-spawn footing, basic target/plan-turn proof, exposed/focused withdrawal proof, and shadow/strike proof are green; next prove the no-omniscience behavior scene, then run the mixed-hostile metrics playtest when the proof gate allows it.

Current green support/behavior probes:
- `writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/` (spawn/save/active-monster footing only).
- `writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/` (target acquisition + `live_plan` seam only).
- `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/` (harness-only noon setup + saved time audit + bright/focused withdrawal live-plan line + save/writeback/active-monster audit).
- `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/` (harness-only vulnerable-midnight setup + saved time audit + shadow-before-strike/strike/cooldown live-plan lines + save/writeback/active-monster audit).

Initial greenlit behavior/perf probes still open:
- `writhing_stalker.live_no_omniscient_beeline_mcw`
- `performance.mixed_hostile_stalker_horde_mcw` — mixed hostile metrics report per `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`; keep queued behind the behavior-scene proof instead of using perf as a substitute.

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
