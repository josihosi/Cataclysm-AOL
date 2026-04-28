# Andi lane: bandit camp-map risk/reward dispatch v0 (2026-04-28)

Status: READY / NOT YET UNLEASHED

## Canon anchors

- Plan item: `Bandit camp-map risk/reward dispatch planning packet v0`
- Full contract: `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`
- Ledgers: `Plan.md`, `SUCCESS.md`, `TESTING.md`

## Active instruction when promoted

Implement the bandit camp-map risk/reward dispatch packet through the real game path. Do not close it as an evaluator-only helper.

The feature is not “make raids bigger.” The feature is: scouts create persistent camp knowledge, camps size follow-up action from current live roster/risk/reward, bandits scout/stalk like living threats, and the running game proves it.

## Implementation scope

1. Inspect and name the existing live hooks before coding:
   - site roster/member state;
   - dispatch cadence;
   - scout-return writeback;
   - local sight-avoidance;
   - serialization;
   - debug/report/log output.
2. Add persisted camp-map remembered leads:
   - target position/id;
   - bounty;
   - threat;
   - confidence;
   - age/last seen;
   - scout outcome/source;
   - scout-seen / target-alert flags;
   - status such as active/stale/invalidated/exploited if useful.
3. Add deterministic roster/reserve/risk/reward helper(s):
   - living roster;
   - ready-at-home count;
   - wounded/unready count;
   - active outside/contact/returning/stalking members;
   - required home reserve;
   - dispatchable count;
   - reward score;
   - risk score;
   - selected intent;
   - selected member count.
4. Change ordinary camp/basecamp scout observation:
   - stand-off target is two OMT from target OMT, not five;
   - reports fallback distance/reason if exact placement fails;
   - ordinary scout watch lasts about half an in-game day / named `720` minute clock;
   - scout returns home and writes memory unless explicitly interrupted.
5. Tighten sight-avoidance:
   - exposed scout/stalker tries non-teleport reposition immediately if possible;
   - by at most two visible local turns, the bandit must have moved, be aborting/returning, or report blocked/no-cover;
   - repeated exposure raises target-alert/caution.
6. Wire remembered-lead consumption into the real dispatch cadence:
   - vanished live signal must not erase scout-confirmed target memory;
   - later cadence can choose hold, re-scout, stalk/pressure, toll/shakedown, raid, or stale/return;
   - selected members transition through real member states.
7. Add reviewer-readable reports/logs for every decision input and outcome.

## Roster and sizing defaults

Use current live roster, not fixed camp folklore.

Reserve defaults:

- `living_roster <= 1`: no hostile dispatch.
- `living_roster == 2`: reserve `1`; scout only unless future explicit desperation logic says otherwise.
- `living_roster == 3-4`: reserve `1`, plus `+1` if recent bandit losses or target-alert risk are high.
- `living_roster == 5-7`: reserve `2`.
- `living_roster >= 8`: reserve `ceil(living_roster * 0.35)`, at least `3`.
- Recent bandit losses / target-alert can add caution/reserve.
- Stockpile desperation can reduce reserve by `1`, but never below hard minimum and normally never below `2` for camps with `5+` living members.

Intent and size defaults:

- `scout`: `1`.
- `re_scout`: `1`, rarely `2` only if terrain/threat justifies it and reserve holds.
- `stalk_pressure`: at least `2` when possible; suggested `clamp(ceil(dispatchable * 0.40), 2, min(dispatchable, ceil(living_roster * 0.35)))`.
- `toll_shakedown`: suggested `clamp(ceil(dispatchable * 0.50), 2, min(dispatchable, ceil(living_roster * 0.45)))`.
- `raid`: suggested `clamp(ceil(dispatchable * 0.65), 2, min(dispatchable, ceil(living_roster * 0.60)))`, only if raid gates pass.
- If the selected intent cannot meet minimum size while preserving reserve, downgrade intent.

Decision shape:

- low confidence / stale memory -> scout or re-scout;
- bad margin -> hold/stale;
- uncertain but valuable -> stalk/pressure;
- good reward/risk -> stalk/pressure or toll/shakedown;
- excellent reward/risk with enough force -> raid;
- high threat alone never escalates by itself.

## Non-goals

- No global faction war planner.
- No parallel same-camp same-target dogpile.
- No full-camp emptying.
- No automatic revenge escalation.
- No five-OMT ordinary scout default.
- No teleporting to satisfy sight-avoidance.
- No deterministic-only closure for a live game claim.

## Deterministic proof required

At minimum:

- roster/reserve table for 2, 4, 5, 7, and 10 living-member camps;
- killed/wounded members reduce future dispatch and can increase caution;
- active same-target outside/contact/stalk/returning group blocks dogpile;
- two-OMT scout stand-off replaces five-OMT default;
- half-day scout-watch timeout returns home and writes memory;
- remembered lead survives vanished live signal;
- high bounty/manageable risk selects larger-than-scout follow-up while preserving reserve;
- high threat/poor reward holds or re-scouts;
- stockpile pressure increases willingness without breaking reserve/risk gates;
- prior defender losses and bandit losses bias later decisions in opposite directions;
- stalk/pressure waits for opening and returns/holds if none appears;
- sight-avoid repositions/aborts within max two visible turns without teleporting;
- blocked/no-cover sight-avoid case reports honestly.

## Harness/product proof required

Use named scenarios from the full contract:

1. `bandit.camp_map_scout_memory_two_omt_watch`
2. `bandit.camp_map_vanished_signal_redispatch`
3. `bandit.variable_roster_dispatch_sizing_live`
4. `bandit.stalk_pressure_waits_for_opening`
5. `bandit.scout_stalker_sight_avoid_live`

Every scenario needs proof-freeze discipline: precondition, action, expected state, screenshot or exact metadata/log proof, failure rule, artifact path, and pass/yellow/red/blocked verdict.

## Completion bar

This lane is done only when:

- implementation exists on `dev`;
- deterministic tests/build are green;
- serialization/writeback/live dispatch path are proven;
- named harness/product proof reaches the running game path;
- evidence classes are honest;
- `Plan.md`, `SUCCESS.md`, `TESTING.md`, and this lane doc are updated from the result.

If proof fails after bounded attempts, do not launder the feature as done. Package implemented-but-unproven state with exact blocker and move according to the current attempt policy.
