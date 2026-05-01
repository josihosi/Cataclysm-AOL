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

### Closed validation receipt - CAOL-ZOMBIE-RIDER-0.3-v0

`CAOL-ZOMBIE-RIDER-0.3-v0` is closed/checkpointed green v0 initial dev. Contract: `doc/zombie-rider-0.3-initial-dev-packet-v0-2026-04-30.md`; imagination source: `doc/zombie-rider-0.3-imagination-source-of-truth-2026-04-30.md`; benchmark suite: `doc/zombie-rider-map-ai-funness-benchmark-suite-v0-2026-04-30.md`; raw intake: `doc/zombie-rider-raw-intake-2026-04-30.md`; closure readout: `doc/zombie-rider-0.3-closure-readout-v0-2026-05-01.md`.

Credited footing evidence:
- Commit `d50715f00e` adds `mon_zombie_rider` with exact three-line Josef description, huge/scary-fast monster footing, pseudo `zombie_rider_bone_bow`, direct mature `GROUP_ZOMBIE` gate at `730 days`, and no normal predator/upgrade path into rider.
- Passage-seam proof `zombie_rider_large_body_small_passage_pathing` proves spawned rider actual size rejects `TFLAG_SMALL_PASSAGE` / `t_window_empty`, normal floor remains valid, normal-sized pathfinding can use the passable window seam, and rider-sized pathfinding routes around it.
- Local combat checkpoint `4343dbdad1` proves live `monster::plan()` consumption for bow shot cadence/cooldown, post-shot reposition destination, close-pressure retreat, injured withdrawal, blocked-LOS no-shot counterplay, and preservation of endpoint footing.
- Overmap light checkpoint `d2ffbd54c3` proves mature-world light attraction, early-world suppression, no-rider/no-light/weak/daylight controls, temporary memory decay, and `max_riders_drawn_by_light = 2`.
- Rider convergence/band checkpoint `ce05ef44ec` proves capped deterministic convergence, decayed-memory/no-eligible/lone-rider controls, and camp pressure postures that include `circle_harass` without wall-suicide.

Credited staged-but-live rows:
- `zombie_rider.live_open_field_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_013055/` (open-field bow pressure then withdrawal).
- `zombie_rider.live_wounded_disengagement_mcw` -> `.userdata/dev-harness/harness_runs/20260501_014613/` (wounded rider withdraws, no bow pressure).
- `zombie_rider.live_cover_escape_mcw` -> `.userdata/dev-harness/harness_runs/20260501_021656/` (opaque cover keeps target/no-sight/no-line-of-fire, no bow pressure).
- `zombie_rider.live_camp_light_band_mcw` -> `.userdata/dev-harness/harness_runs/20260501_030432/` (camp lights aggregate into capped two-rider band/circle-harass).
- `zombie_rider.live_no_camp_light_control_mcw` -> `.userdata/dev-harness/harness_runs/20260501_032016/` (matched no-light footing emits no horde-light or rider camp-light/band trace).

Caveat: staged-but-live McWilliams rows, not natural random discovery/full siege navigation/release packaging. Do not rerun solved rider rows by ritual unless Schani/Josef explicitly promote a stricter follow-up.

### Closed validation receipt - CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0

`CAOL-FLESH-RAPTOR-CIRCLING-SKIRMISHER-v0` is closed/checkpointed green v0. Contract / closure evidence: `doc/flesh-raptor-circling-skirmisher-packet-v0-2026-05-01.md`; imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Credited deterministic footing:
- Code seam: `src/monmove.cpp` routes only base/shadow/unstable/electric/dusted/fungalized/fungal flesh raptors through `apply_flesh_raptor_plan()` after existing writhing-stalker and zombie-rider special plan hooks, before generic target pursuit.
- Helper: `src/flesh_raptor_ai.*` scores 4–6 tile lateral/orbit candidates, under-occupied arcs, held-destination hysteresis, bounded cadence swoop windows, and fallback when no readable lateral orbit exists.
- Focused tests: `tests/flesh_raptor_test.cpp` covers variant footing, lateral-over-straight scoring, crowding preference, cadence/hysteresis, corridor fallback, live `monster::plan()` consumption for `mon_spawn_raptor`, and preservation of a non-raptor `HIT_AND_RUN` monster.

Green gates:
- `make -j4 tests/flesh_raptor_test.o tests src/flesh_raptor_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[flesh_raptor]"` -> `All tests passed (61 assertions in 7 test cases)`.
- `make -j4 TILES=1 LINTJSON=0 ASTYLE=0` -> green after the committed-swoop movement bridge fix.
- `git diff --check` -> clean.
- `make astyle-diff LINTJSON=0 ASTYLE=0` could not run because `astyle` is not installed on this host.

Credited staged-but-live rows:
- `flesh_raptor.live_open_field_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_052709/` proves feature-path open-field orbit/swoop behavior with step ledger green (`6/6`), `artifacts_matched`, `feature_proof True`, no runtime warnings, and required log state present.
- `flesh_raptor.live_crowded_arc_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_053414/` proves feature-path crowded-arc behavior: a staged raptor avoids the zombie-crowded north arc and selects the under-occupied south/open arc before bounded swoops.
- `flesh_raptor.live_blocked_corridor_skirmisher_mcw` -> `.userdata/dev-harness/harness_runs/20260501_054807/` proves feature-path blocked/corridor behavior: lateral/behind orbit sample tiles sealed, east lane open, `decision=fallback reason=no_readable_lateral_orbit`, then bounded corridor swoop/committed path pressure.
- `flesh_raptor.live_equipment_frustration_comparison_mcw` -> `.userdata/dev-harness/harness_runs/20260501_062300/` proves the old-feeling/equipment-damage comparison row with step ledger green (`7/7`), `artifacts_matched`, `feature_proof True`, clean startup, no runtime warnings, cleanup terminated, current orbit/swoop/melee debug metrics, and screenshot/OCR player-facing evidence: `flesh-raptor`, `impales`, `cut!`, `You put pressure on the bleeding wound...`, and `You're bleeding!`.

Closure verdict: Frau accepted v0 for agent-side close with staged-but-live caveats. Optional Josef taste/playtest remains future-only and is not a blocker. No equipment-damage tuning changed; equipment damage remains an observational frustration metric.

### Active validation target - CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0

`CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` is active after writhing-stalker seam-reduction closure. Contract: `doc/camp-locker-equipment-api-reduction-packet-v0-2026-05-01.md`; imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Current validation burden:
- First coverage-helper reduction is green: `git diff --check`; `make -j4 tests/faction_camp_test.o tests src/basecamp.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[camp][locker]"` -> `All tests passed (2062 assertions in 71 test cases)`.
- Zone-boundary reduction is green: `collect_camp_locker_zone_items` now defers the pickup boundary to the existing `NO_NPC_PICKUP` zone API before considering `CAMP_LOCKER` stock; `camp_locker_zone_candidate_gathering` covers the overlapped-zone regression; `./tests/cata_test "[camp][locker]"` -> `All tests passed (2062 assertions in 71 test cases)`.
- Medical-readiness reduction is green: camp locker readiness/kept-supply logic now recognizes bandage/bleed supplies through existing `heal_actor` use-action metadata instead of a camp-local item id list; `camp_locker_service_readies_medical_consumables_from_locker_supply`, `camp_locker_service_preserves_carried_medical_and_caps_reserve`, and `camp_locker_service_ignores_unrelated_medical_consumables` -> `All tests passed (37 assertions in 3 test cases)`; full `./tests/cata_test "[camp][locker]"` after this slice -> `All tests passed (2066 assertions in 71 test cases)`.
- Continue auditing remaining camp locker classification/scoring, carried cleanup, and ranged ammo/magazine readiness against existing item, wearability, reload, and zone APIs.
- Any code refactor must preserve camp locker policy behavior: enabled slots, bulletproof/weather-sensitive preference, readiness supplies, camp-storage boundaries, and safe leftover returns.
- Targeted tests should continue to cover camp locker classification/upgrade selection, carried cleanup, magazine/ammo readiness, and any newly touched `CAMP_STORAGE` / `NO_NPC_PICKUP` boundary behavior.
- Do not claim basecamp mission redesign, Smart Zone redesign, or outfit tuning from this cleanup unless separately promoted and proven.

### Closed validation receipt - CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0

`CAOL-WRITHING-STALKER-BEHAVIOR-SEAM-REDUCTION-v0` is closed/checkpointed green v0. Contract: `doc/writhing-stalker-behavior-seam-reduction-packet-v0-2026-05-01.md`; imagination source: `doc/anti-redundancy-packaging-imagination-source-of-truth-2026-05-01.md`.

Credited evidence:
- Inventory checkpoint recorded the live path: `monster::plan()` target acquisition -> writhing-stalker evaluator/quiet-side scorer -> destination/effect side effects.
- Seam-consumption proof `writhing_stalker_live_plan_consumes_quiet_side_cutoff_seam` calls live `monster::plan()` and fails if the live planner stops consuming quiet-side cutoff behavior.
- Source refactor in `src/monmove.cpp` routes writhing stalker / zombie rider / flesh raptor target-response planning through a named `targeted_live_plan_adapter` dispatch before the generic hostile/flee destination fallback. No behavior-tree or special-attack seam honestly owns this destination-planning response today, so product-specific stalker judgment stays custom.

Gates: `git diff --check`; `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker]"` -> `All tests passed (192 assertions in 15 test cases)`; adapter spillover guard `./tests/cata_test "[zombie_rider],[flesh_raptor]"` -> `All tests passed (231 assertions in 21 test cases)`.

### Closed validation receipt - CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0

`CAOL-WRITHING-STALKER-ZOMBIE-SHADOW-PREDATOR-v0` is closed/checkpointed green v0 after flesh-raptor closure. Contract: `doc/writhing-stalker-zombie-shadow-predator-packet-v0-2026-05-01.md`; imagination source: `doc/predator-behavior-variety-imagination-source-of-truth-2026-05-01.md`.

Credited deterministic footing:
- Code seam: `src/writhing_stalker_ai.*` adds explicit confidence components for evidence/interest, zombie pressure, quiet-side/cutoff opportunity, and counterpressure from light/focus/open exposure; `src/monmove.cpp` routes live shadow destinations through `choose_quiet_side_cutoff()` and logs confidence alongside opportunity.
- Focused tests in `tests/writhing_stalker_test.cpp` prove east-side zombie pressure prefers west/quiet-side shadow candidate, split east/west pressure avoids fake precision, retreat-route bias can choose a consistent quiet cutoff, zombie pressure is ignored without local evidence or overmap-interest footing, overmap-interest footing can admit pressure confidence, and light/focus/open exposure suppresses cutoff confidence.
- Preservation tests still cover no-evidence/no-magic targetlessness, cooldown anti-spam, light/focus withdrawal, injured retreat, repeated strike rhythm, and old monster/spawn footing.

Credited staged-but-live rows:
- `writhing_stalker.live_quiet_side_zombie_pressure_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071548/` proves the scoped local-evidence-only first live row: `overmap_interest=no`, `zombie_pressure=3`, east/front pressure (`pressure_x=3`) maps through the live shadow-destination path to west/quiet-side cutoff (`quiet_x=-1`, first matched `chosen_rel_x=-1`, `chosen_rel_y=-4`, reason `quiet_side_cutoff_preferred`). Proof note: `doc/writhing-stalker-zombie-shadow-live-quiet-side-proof-v0-2026-05-01.md`.
- `writhing_stalker.live_escape_side_zombie_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260501_071940/` proves the scoped local-evidence-only retreat row: after southward retreat input, `overmap_interest=no`, `zombie_pressure=3`, north/front pressure (`pressure_y=-3`) maps through the live shadow-destination path to south/escape-side cutoff (`quiet_y=1`, first matched `chosen_rel_y=4`, reason `quiet_side_cutoff_preferred`). Proof note: `doc/writhing-stalker-zombie-shadow-live-escape-side-proof-v0-2026-05-01.md`.

Green gates:
- `make -j4 tests/writhing_stalker_test.o tests src/writhing_stalker_ai.o LINTJSON=0 ASTYLE=0 && ./tests/cata_test "[writhing_stalker][ai]"` -> `All tests passed (135 assertions in 12 test cases)`.
- `./tests/cata_test "[writhing_stalker]"` -> `All tests passed (167 assertions in 14 test cases)`.

Closure verdict:
- Deterministic zombie-pressure / quiet-side footing is green.
- Both scoped live rows are green and explicitly **do not** claim overmap-interest footing.
- V0 proves local-evidence live zombie-shadow behavior; live overmap-interest wiring/logging remains unclaimed unless a later packet promotes it.
- Old closed writhing-stalker constraints remain preserved: no omniscience, no zombie-proximity magic target acquisition, cooldown anti-spam, light/focus counterplay, and injured retreat.
- Anti-redundancy cleanup is now a separate active refactor lane and must preserve this approved behavior rather than reopening product tuning.


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

Active same-lane next action is the remaining audit/refactor pass for `CAOL-CAMP-LOCKER-EQUIPMENT-API-REDUCTION-v0` after the coverage-helper, zone-boundary, and medical-readiness reductions.

Required remaining audit questions:
- Which item classification/scoring, carried cleanup, and ranged ammo/magazine readiness checks are duplicate engine ontology rather than camp policy?
- Which existing item, wearability, body coverage, reload, and zone APIs can safely own those checks?
- Which behavior is deliberately camp-specific policy and must remain explicit: enabled slots, bulletproof/weather-sensitive preference, readiness supplies, camp-storage boundaries, and safe leftover returns?
- What is the smallest focused faction/basecamp/camp-locker gate that proves preservation after the next refactor?

Closed zombie-rider, flesh-raptor, writhing-stalker, roof-horde, Smart Zone, fire, bandit, and multi-camp proof trains are represented by `SUCCESS.md`, `Plan.md`, `doc/work-ledger.md`, linked aux proof docs, and git history. Do not rerun solved rows as ritual.

Future-only watchlist unless Schani/Josef explicitly promotes it:
- Natural three/four-site player-pressure behavior and true zero-site idle baseline remain decision/watchlist items, not current requirements.
- A stricter roof-horde positive `tracking_intensity` proof remains optional/future-only; current green roof proof is retarget/movement-budget metadata after live roof-fire horde signaling.
- Full Smart Zone process-reload disk persistence can be promoted later if Josef wants that stricter audit; the current green proof covers live create/inspect/close-save/reopen coordinate labels.

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
