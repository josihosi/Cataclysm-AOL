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

### Closed validation receipt - CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0

`CAOL-WRITHING-STALKER-LIVE-FUN-SCENARIOS-v0` is closed/checkpointed green v0. Contract: `doc/writhing-stalker-live-fun-scenarios-packet-v0-2026-04-30.md`; imagination source: `doc/writhing-stalker-live-fun-scenarios-imagination-source-of-truth-2026-04-30.md`; prior deterministic proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`; closure proof: `doc/writhing-stalker-live-fun-scenario-proof-v0-2026-04-30.md`.

Credited evidence:
- `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/` proves bright/focused light counterplay withdraw/cooling and forbids strike.
- `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/` proves cover-shadow approach, close vulnerability strike, cooldown, and re-engagement without spam.
- `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/` proves zombie-pressure-assisted shadow/strike/cooldown rhythm only with evidence/vulnerability.
- `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/` proves nearby zombies do not grant target acquisition through the opaque wall.
- `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/` proves light/focus escape breaks pressure into withdraw/cooling and forbids strike.
- `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/` proves injured retreat overrides greed with no strike snapback.

Gates: `make -j4 TILES=1 LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (129 assertions in 10 test cases)`. Live cooldown in `src/monmove.cpp` was minimally adjusted so `cooling_off` no longer refreshes `effect_run` forever.

Caveat: staged-but-live McWilliams scenarios, not fully natural random discovery; future manual discovery is optional/future-only unless promoted.

### Active validation target - CAOL-ZOMBIE-RIDER-0.3-v0

`CAOL-ZOMBIE-RIDER-0.3-v0` is active after the writhing-stalker live fun packet closed green. Monster/evolution, local combat AI, overmap light-attraction, deterministic rider convergence/band pressure footing, the first staged-but-live open-field local-combat row, the staged-but-live wounded disengagement row, the staged-but-live cover/LOS escape row, and the staged-but-live camp-light convergence/band row are green; next validation slice is still live-shaped rider funness rows. Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`; imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`; benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`; raw intake: `doc/zombie-rider-raw-intake-2026-04-30.md`.

Credited footing evidence:
- Commit `d50715f00e` adds `mon_zombie_rider` with exact three-line Josef description, huge/scary-fast monster footing, pseudo `zombie_rider_bone_bow`, direct mature `GROUP_ZOMBIE` gate at `730 days`, and no normal predator/upgrade path into rider.
- Gates: `make -j4 tests/zombie_rider_test.o tests LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[zombie_rider]"` -> `All tests passed (34 assertions in 2 test cases)` with clean JSON load/no debug errors.
- Follow-up passage-seam gate: `zombie_rider_large_body_small_passage_pathing` proves spawned rider actual size rejects `TFLAG_SMALL_PASSAGE` / `t_window_empty`, normal floor remains valid, normal-sized pathfinding can use the passable window seam, and rider-sized pathfinding routes around it. Focused run: `./tests/cata_test "[zombie_rider]"` -> `All tests passed (59 assertions in 3 test cases)`.
- Local combat AI checkpoint `4343dbdad1` adds live `monster::plan()` consumption plus focused tests for bow shot cadence/cooldown, post-shot reposition destination, close-pressure retreat, injured withdrawal, blocked-LOS no-shot counterplay, and preservation of endpoint footing. Gates: `make -j4 tests/zombie_rider_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[zombie_rider]"` -> `All tests passed (100 assertions in 7 test cases)`.
- Overmap light-attraction checkpoint `d2ffbd54c3` adds `zombie_rider_overmap_ai` over existing `bandit_mark_generation::light_projection` / `horde_signal_power_from_light_projection` seams, with mature-world gating, no-rider/no-light/weak/daylight controls, temporary memory decay, and `max_riders_drawn_by_light = 2`. Gates: `make -j4 tests/zombie_rider_test.o obj/zombie_rider_overmap_ai.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[zombie_rider]"` -> `All tests passed (135 assertions in 10 test cases)`. `make astyle-diff` is not available on this host because `astyle` is missing.
- Rider convergence/band checkpoint `ce05ef44ec` adds deterministic loose-rider selection by overmap distance then `rider_id`, ignores unavailable/cooling/already-banded/out-of-radius riders, preserves `max_riders_drawn_by_light = 2`, keeps lone riders below band minimum, suppresses convergence after decayed memory, and chooses camp pressure postures (`none`, `investigate`, `circle_harass`, `direct_attack`, `withdraw`) without wall-suicide. Gates: `git diff --check && make -j4 tests/zombie_rider_test.o obj/zombie_rider_overmap_ai.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[zombie_rider]"` -> `All tests passed (170 assertions in 14 test cases)`.
- Live open-field local-combat row `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/` is green after fresh runtime build `make -j4 TILES=1 LINTJSON=0 ASTYLE=0`: one hostile `mon_zombie_rider` staged at offset `[0,-5,0]`, noon start audited, safe mode off, `6/6` step rows green, no abort/runtime warnings, artifact rows include `decision=bow_pressure reason=line_of_fire distance=5 ... eval_us=2` then `decision=withdraw ... distance=3 ... eval_us=1`; decision counts `bow_pressure=1`, `withdraw=2`, max observed `eval_us=2`. Proof note: `doc/zombie-rider-live-funness-open-field-proof-v0-2026-05-01.md`.
- Live wounded disengagement row `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/` is green after forced fresh executable relink `rm -f obj/version.o cataclysm-tiles && make -j4 TILES=1 LINTJSON=0 ASTYLE=0`: one wounded hostile `mon_zombie_rider` staged at offset `[0,-8,0]`, noon start audited, safe mode off, runtime title `7ecdd41f03-dirty` with matching runtime paths, `6/6` step rows green, no abort/runtime warnings, artifact rows include `decision=withdraw reason=wounded_or_close distance=7 line_of_fire=yes hp=36 ... eval_us=1` through `distance=24`, decision counts `withdraw=14`, `bow_pressure=0`, max observed `eval_us=3`. Proof note: `doc/zombie-rider-live-funness-wounded-disengagement-proof-v0-2026-05-01.md`.
- Live cover/LOS escape row `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/` is green: one hostile `mon_zombie_rider` staged at offset `[10,0,0]`, noon start audited, overmap NPCs removed/audited as zero, thick opaque `f_locker` wall guard offsets present, safe mode off, `9/9` step rows green, no abort/runtime warnings, feature classification `feature-path`, artifact rows include `zombie_rider target_probe: target=no sees_player=no distance=10 line_of_fire=no hp=100 run=no eval_us=1`; counts `target_probe=104`, `decision=bow_pressure=0`, `line_of_fire=yes=0`. Proof note: `doc/zombie-rider-live-funness-cover-escape-proof-v0-2026-05-01.md`.
- Live camp-light convergence/band row `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/` is green after the fresh `cataclysm-tiles` relink: mature-world midnight fixture (`world_age_days=735`), zero overmap NPC targets, two hostile riders staged at `[20,20,0]` and `[-20,20,0]`, four bright `fd_fire` camp lights, safe mode off, `8/8` step rows green, no yellow/red/blocked rows, feature classification `feature-path`, same-run artifact row includes `aggregate_sources=3`, `aggregate_horde_signal_power=66`, `interest=yes`, `selected_riders=2`, `band_formed=yes`, `band_size=2`, and `posture=circle_harass`. Proof note: `doc/zombie-rider-live-funness-camp-light-proof-v0-2026-05-01.md`.

Validation burden:
- Keep the existing endpoint evolution/spawn gate, large-body passage seam, scary-fast movement constraints, local shoot/flee/reposition constraints, bounded light-interest constraints, and deterministic convergence/band pressure constraints green while widening into live-shaped rows.
- Live/harness playtests green so far: open-field local-combat pressure, wounded-rider disengagement, cover/LOS escape, and camp-light attraction into capped rider-band circle-harass.
- Next live slice should widen camp-light control/counterplay or another funness row only if the harness can stage the claim honestly; natural random discovery/full siege behavior remains future-only unless promoted.
- Report scenario/test/run ids, start conditions, decision traces, rider counts, band state, shot/reposition/retreat counts, pass/fail/yellow verdicts, caveats, stability/perf, and crash/log-spam state.
- Do not close on JSON/stat presence alone; gameplay claims must reach tests and/or live-shaped scenarios.

### Closed validation target - CAOL-WRITHING-STALKER-PATTERN-TESTS-v0

`CAOL-WRITHING-STALKER-PATTERN-TESTS-v0` is closed/checkpointed green v0. Contract: `doc/writhing-stalker-behavior-pattern-minimap-packet-v0-2026-04-30.md`; imagination source: `doc/writhing-stalker-behavior-pattern-imagination-source-of-truth-2026-04-30.md`; proof: `doc/writhing-stalker-behavior-pattern-proof-v0-2026-04-30.md`.

Credited evidence:
- Deterministic pattern helper in `tests/writhing_stalker_test.cpp`: `stalker_pattern_row`, `trace_rows`, `pattern_base_context`, `run_vulnerable_stalker_pattern`, `count_decisions`.
- Green tests: `make -j4 tests/writhing_stalker_test.o tests LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` -> `All tests passed (97 assertions in 8 test cases)`.
- Broader focused filter: `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (129 assertions in 10 test cases)`.
- Trace extraction: `./tests/cata_test "writhing_stalker_pattern_helper_traces_repeated_strikes_then_injured_retreat" -s` -> `All tests passed (16 assertions in 1 test case)`.

Credited behavior-pattern facts:
- no believable local evidence -> `ignore`, `live_no_believable_evidence`;
- weak evidence latch times out via `latch_timed_out`;
- cover route is preferred over direct open route;
- bright/focused exposure withdraws with `live_exposed_and_focused_withdraw`;
- zombie pressure alone does not create a strike;
- vulnerability can create `live_vulnerability_window_strike`;
- repeated trace shows `shadow -> strike -> cooling_off -> cooling_off -> shadow -> strike -> withdraw`;
- strike count `2`, retreat trigger `hp=50`, jitter count `0`.

Caveat: this is deterministic helper proof, not a new live harness scene. No behavior code changed; the existing live seam remains `monster::plan()` -> `apply_writhing_stalker_plan()` -> `writhing_stalker::evaluate_live_response()`.

### Closed validation receipt - CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0

`CAOL-MULTI-CAMP-SIGNAL-GAUNTLET-v0` is closed/checkpointed green v0. Contract: `doc/multi-camp-signal-gauntlet-playtest-packet-v0-2026-04-30.md`; imagination source: `doc/multi-camp-signal-gauntlet-imagination-source-of-truth-2026-04-30.md`; proof: `doc/multi-camp-signal-gauntlet-proof-v0-2026-04-30.md`.

Green harness runs:
- Challenge A: `bandit.multi_camp_structural_stress_mcw`, `.userdata/dev-harness/harness_runs/20260430_204416/` — two-camp structural stress, bounded wait, spread/no-repeat/followthrough, and perf/log stability.
- Challenge B: `bandit.mixed_signal_coexistence_mcw`, `.userdata/dev-harness/harness_runs/20260430_203757/` — live smoke/fire signal plus separate structural bounty, `candidate_reason=live_signal`, both state classes preserved.
- Challenge C: `bandit.mixed_signal_reload_resume_mcw`, `.userdata/dev-harness/harness_runs/20260430_203944/` — no-fixture reload/resume of active live-signal scout and structural scavenge groups.

Do not reopen this target without explicit Schani/Josef promotion. Caveats: two camps rather than four; staged-but-live signal source; Challenge A is not an all-camps-idle-after-harvest proof.


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

Active same-lane next probe is `CAOL-ZOMBIE-RIDER-0.3-v0` camp-light attraction into bounded convergence/band circling, if the harness can stage it honestly. Old `CAOL-WRITHING-STALKER-v0`, deterministic pattern tests, writhing-stalker live fun scenarios, nice roof-fire horde, Smart Zone, old fire proof lanes, and the multi-camp signal gauntlet remain closed and must not be reopened by drift.

Current zombie-rider green live rows:
- `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/` (open-field bow pressure then withdrawal).
- `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/` (wounded rider withdraws, no bow pressure).
- `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/` (opaque cover keeps target/no-sight/no-line-of-fire, no bow pressure).

Closed writhing-stalker green support/behavior probes:
- `writhing_stalker.live_spawn_footing_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161342/` (spawn/save/active-monster footing only).
- `writhing_stalker.live_plan_seam_mcw` -> `.userdata/dev-harness/harness_runs/20260430_161535/` (target acquisition + `live_plan` seam only).
- `writhing_stalker.live_exposed_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260430_163626/` (earlier bright/focused withdrawal support row).
- `writhing_stalker.live_shadow_strike_mcw` -> `.userdata/dev-harness/harness_runs/20260430_170528/` (earlier vulnerable shadow/strike/cooldown support row).
- `writhing_stalker.live_no_omniscient_beeline_mcw` -> `.userdata/dev-harness/harness_runs/20260430_173555/` (earlier no-evidence/no-target support row).
- `writhing_stalker.live_campfire_counterplay_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233129/` (live fun Scenario A).
- `writhing_stalker.live_alley_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233156/` (live fun Scenario B).
- `writhing_stalker.live_zombie_distraction_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233521/` (live fun Scenario C positive row).
- `writhing_stalker.live_zombie_distraction_no_magic_guard_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233335/` (live fun Scenario C no-magic guard).
- `writhing_stalker.live_door_light_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233405/` (live fun Scenario D).
- `writhing_stalker.live_wounded_predator_mcw` -> `.userdata/dev-harness/harness_runs/20260430_233434/` (live fun Scenario E).
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
