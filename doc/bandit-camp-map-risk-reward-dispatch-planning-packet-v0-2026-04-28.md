# Bandit camp-map risk/reward dispatch planning packet v0 (2026-04-28)

Status: ACTIVE / GREENLIT IMPLEMENTATION + LIVE PRODUCT MATRIX

Source-of-truth framing: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`.
Code-only implementation map: `doc/bandit-camp-map-ecology-implementation-map-2026-04-28.md`.
Bandit live product playtest matrix: `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`.

This packet is downstream of the camp-map ecology picture: dispatch/risk/reward are the hands; the saved per-camp map is the creature.

## Why this exists

The current scout-return loop is too disposable if it requires a fresh live signal before the camp can care again. That does not read like bandits. If a scout finds a player/basecamp site, the source camp should remember it on a bandit-side camp map as a lead with bounty, threat, confidence, age, source, and scout outcome. The original smoke/light/noise signal can vanish; the scouted target should remain actionable until later evidence decays, confirms, exploits, or invalidates it.

Audit boundary from the pre-lane implementation: existing scout-return evidence only proves return-home cleanup / pressure refresh and member writeback. It does not prove this camp-map lead contract. Do not treat `scout_report: returned -> pressure refreshed`, `active_*` field reset, or `remembered_bounty_estimate`/`remembered_threat_estimate` integers alone as closure for persistent scout-confirmed target memory.

Save/persistence boundary: current `bandit_live_world` site serialization proves a saved per-site ledger substrate, not the required per-camp intelligence map. The lane must implement or explicitly name the camp-owned map structure and prove it survives the normal game save/load path with multiple camps keeping independent leads.

The second missing piece is dispatch sizing. A camp with five bandits, two bandits, or twelve bandits cannot use the same magic response size. Dispatch must come from the current live roster: who is alive, home, ready, wounded, already outside, recently lost, and needed for home reserve. High threat alone must not mean “send more bandits.” The camp should weigh risk and reward, then pick an intent and a member count.

The third missing piece is behavior texture. A scout should not sit five overmap tiles away like a nervous weather balloon. For ordinary camp/basecamp observation, the first scout should aim for a two-OMT stand-off from the target OMT, meaning one full OMT remains between scout and camp. The scout watches for about half an in-game day, then returns home and writes the lead. If the camp does not attack immediately, the next pressure beat can be a bigger stalk/pressure dispatch that waits for an opening rather than collapsing straight into a fight.

## Scope

1. Add an explicit independent bandit-side intelligence map owned by each camp and persisted with the game save.
2. Add a persistent camp-map lead for scout-confirmed targets:
   - target id / approximate overmap position;
   - bounty estimate;
   - threat estimate;
   - confidence;
   - last seen / age / source scout outcome;
   - scout-seen / target-alert flags;
   - optional stale / confirmed / invalidated / exploited status.
3. Make scout return write this remembered lead even when the original live signal is gone.
4. Let the real live dispatch cadence re-plan from remembered scout leads, not only from currently visible live signal packets. This must be wired through the actual game path that currently evaluates owned hostile sites, not left as an isolated evaluator helper.
5. Replace the default ordinary five-OMT scout hold-off with a two-OMT scout-watch envelope for camp/basecamp observation, with a named constant or report field for the selected stand-off distance.
6. Give ordinary scout watches a bounded half-day sortie clock, nominally `720` in-game minutes, then return home and write memory unless a higher-priority contact/attack/abort condition honestly interrupts it.
7. Tighten local sight-avoidance for scouts/stalkers: if the player or camp NPCs see the bandit, the bandit must attempt a non-teleport reposition immediately or within at most two local turns, with blocked/no-cover cases reported explicitly.
8. Add a bounded risk/reward dispatch-sizing rule using:
   - live roster / living roster;
   - at-home ready members;
   - wounded/unready members;
   - active outside groups;
   - required home reserve;
   - camp stockpile/need/pressure where available, with a named placeholder if detailed stockpile is not modeled yet;
   - target bounty, threat, confidence, distance, and age;
   - scout-seen / target-alert state;
   - recent defender losses, bandit losses, failed extraction, or successful toll.
9. Choose intent before member count: hold, scout, re-scout, stalk/pressure, toll/shakedown, raid, return/stale.
10. After a returned scout confirms a target but the camp does not attack, allow the risk/reward cadence to choose a larger stalk/pressure dispatch that waits for an opening, while still preserving reserve, anti-dogpile, and high-threat restraint.
11. Make reviewer-readable reports show all decision inputs and the chosen job/member count.
12. Add the code integration points needed for the live game loop: serialization/deserialization of remembered leads, scout-return writeback, dispatch-cadence consumption, selected-member writeback, sight-avoidance state, and report/log output.

## Non-goals

- Do not build a global faction war planner.
- Do not make every vanished signal remain actionable forever.
- Do not let high threat alone mean “send more bandits.”
- Do not allow parallel dogpile dispatch from the same camp while an existing active outside group/contact is unresolved.
- Do not keep the previous five-OMT ordinary scout stand-off as the default for camp/basecamp observation; it remains historical proof of the old correction, not the new target behavior.
- Do not make stalk escalation automatic revenge. It is a risk/reward choice after scout memory, with high-threat/low-reward cases still allowed to hold or re-scout.
- Do not let large camps empty themselves just because a target looks valuable.
- Do not let tiny camps perform serious raids unless a later explicit desperation/personality system justifies it and still names the risk.
- Do not close this from deterministic evaluator tests alone if the claim says the live bandit camp re-dispatches from remembered scout knowledge. The live game path must consume the new memory and produce an in-game/harness-observable result.
- Do not route this as an informal Andi nudge; it is a canon plan item with tests.

## Vielleicht scenario pass

Use these scenarios to shape deterministic logic, not as vague flavor text:

1. **Tiny thieves, too few hands:** two living bandits, one can scout, one stays home. A good target becomes memory; a real raid is normally unavailable.
2. **Small road camp:** three or four living bandits can scout and maybe send a small stalk/toll pair if enough members remain ready. Home reserve still wins over greed.
3. **Careful five-bandit camp, good target:** five living bandits, healthy roster, low-to-moderate threat, high bounty/stockpile need. One scout watches from two OMT for about half a day, returns, writes memory, and the next cadence chooses a two-member stalk/pressure group while leaving home reserve.
4. **Dangerous target, poor reward:** high threat, poor reward, low confidence, or scout seen repeatedly. Scout may observe and return, but the camp holds, re-scouts, marks stale, or cools pressure instead of escalating.
5. **Seen scout, living world:** the player or camp NPC spots the scout/stalker. The bandit moves, not teleports, toward cover/broken line of sight immediately or by turn two; repeated exposure can force abort/return and writes a target-alert flag.
6. **Opening appears:** remembered target is valuable, threat is acceptable, and later observations suggest weak guard/time/weather/open camp behavior. The stalk/pressure group may advance to toll/shakedown/raid where the existing job model allows it.
7. **No opening appears:** stalkers wait within their bounded envelope, decay confidence or return home, and do not hover forever.
8. **Camp is stretched thin:** low at-home count, active outside group, wounds, or required reserve blocks bigger dispatch even if the target looks juicy.
9. **Large raider camp:** eight to twelve living bandits can send a meaningful group, but only after reserve and anti-dogpile gates. It may send scouts/stalkers over time, not simultaneous same-target clown-car attacks.
10. **Loss spiral avoided:** prior bandit losses increase caution/reserve and can shrink future dispatch. Prior defender losses can increase pressure, but only within bounded risk/reward logic.

## Deterministic behavior model

### Roster terms

Use names like these in code/reports even if final C++ names differ:

- `living_roster`: all living members owned by the site.
- `ready_at_home`: members at home who are alive, not badly wounded, not already selected for an active job, and usable for the current job.
- `wounded_or_unready`: living members who should count for camp survival but not dispatch readiness.
- `active_outside`: outbound, local-contact, returning-home, stalk/pressure, raid, or other unresolved external members.
- `hard_home_reserve`: minimum members that must remain home.
- `dispatchable`: `max(0, ready_at_home - hard_home_reserve)` after active-outside/dogpile gates.

### Home reserve default

Default reserve should be deterministic and profile-adjustable, but not hand-wavy:

- `living_roster <= 1`: no hostile dispatch.
- `living_roster == 2`: reserve `1`; scout only unless future explicit desperation logic says otherwise.
- `living_roster == 3-4`: reserve `1`, plus `+1` if recent bandit losses or target-alert risk are high.
- `living_roster == 5-7`: reserve `2`.
- `living_roster >= 8`: reserve `ceil(living_roster * 0.35)`, rounded at least `3`.
- Recent bandit losses or a scout-seen/target-alert flag can add `+1` reserve/caution.
- Severe stockpile pressure/desperation may reduce reserve by `1`, but never below `1`, and never below `2` for camps with `5+` living members unless an explicit later desperation profile says so.

### Intent selection

Choose intent before count:

- `hold`: weak camp, bad risk/reward, unresolved outside group, or no opening.
- `scout`: no confirmed memory, stale memory, or low confidence.
- `re_scout`: memory exists but is old/uncertain or scout was seen and target alert rose.
- `stalk_pressure`: target is valuable enough to watch for an opening, but attack is not yet justified.
- `toll_shakedown`: reward/confidence are good, risk is acceptable, and contact/intimidation is more sensible than direct raid.
- `raid`: only high reward, good confidence, enough dispatchable members, and manageable risk.
- `return_or_stale`: no opening appears within the bounded stalk window, route fails, exposure persists, or memory decays.

### Scores and thresholds

Implementation may tune exact values, but the first version should expose these named inputs in deterministic tests/reports:

- `reward_score`: bounty + confidence + stockpile_pressure + target_weakness/opening hints - distance burden.
- `risk_score`: threat + uncertainty + scout_seen/target_alert + prior_bandit_losses + route danger + zombies/fire/weather + distance burden.
- `margin`: `reward_score - risk_score`.

Default decision shape:

- low confidence / stale lead -> `scout` or `re_scout`.
- `margin <= -2` -> `hold` / mark stale / re-scout only.
- `margin -1..1` -> `stalk_pressure` only if bounty/need is meaningful; otherwise hold.
- `margin 2..4` -> `stalk_pressure` or `toll_shakedown`.
- `margin >= 5` -> `raid` only if reserve, active-outside, and target-alert gates allow it.

### Dispatch size defaults

Sizing is derived from current live roster, not fixed camp folklore:

- `scout`: `1`.
- `re_scout`: `1`; allow `2` only if terrain/threat profile explicitly calls for paired scouting and reserve still holds.
- `stalk_pressure`: at least `2` when possible; otherwise do not pretend a one-bandit stalk is the larger follow-up. Suggested size: `clamp(ceil(dispatchable * 0.40), 2, min(dispatchable, ceil(living_roster * 0.35)))`.
- `toll_shakedown`: suggested size `clamp(ceil(dispatchable * 0.50), 2, min(dispatchable, ceil(living_roster * 0.45)))`.
- `raid`: suggested size `clamp(ceil(dispatchable * 0.65), 2, min(dispatchable, ceil(living_roster * 0.60)))`, and only when the raid intent gates pass.
- Never select more members than `dispatchable`.
- If the calculated minimum cannot be met while preserving reserve, downgrade intent instead of violating reserve.

### Examples

- 2 living / 2 ready: reserve 1, dispatchable 1 -> scout only.
- 4 living / 4 ready: reserve 1, dispatchable 3 -> scout 1; stalk/toll 2 if risk/reward permits; raid usually blocked unless very favorable.
- 5 living / 5 ready: reserve 2, dispatchable 3 -> scout 1; stalk/toll 2; raid 2-3 if very favorable.
- 7 living / 6 ready / 1 wounded: reserve 2, dispatchable 4 -> scout 1; stalk 2; shakedown 2-3; raid up to 4 if very favorable.
- 10 living / 9 ready: reserve 4, dispatchable 5 -> scout 1; stalk 2-3; shakedown 3-4; raid up to 5-6 only if the cap and dispatchable allow it.
- Any camp with active same-target outside group/contact -> no parallel same-target dogpile; resolve, return, or abort first.

### Scout and stalk behavior

For ordinary camp/basecamp observation:

- Initial scout stand-off target is distance `2 OMT` from the target OMT, so one OMT sits between scout OMT and camp/basecamp OMT.
- If terrain/pathing cannot place exactly two OMT, reports must name fallback distance and reason.
- Scout-watch duration is a named half-day window, nominally `720` in-game minutes.
- Scout return writes a remembered lead with bounty/threat/confidence/source/outcome and age.
- Later cadence reads that remembered lead and chooses among hold, re-scout, stalk/pressure, toll/shakedown, or raid.
- A stalk/pressure dispatch should wait for an opening, not instantly attack.
- Opening hints may include target leaving camp, low guard, time-of-day, poor visibility, bad weather, zombie distraction, fire/noise routine, or exposed supplies.
- If no opening appears within a bounded stalk window, stalkers return/hold and confidence decays; they do not hover forever.

### Sight avoidance

- Track consecutive visible/exposed local turns for scout/stalker members.
- At exposure turn `0-1`, try to move immediately if a legal better tile exists.
- By exposure turn `2`, the bandit must have moved, be mid-abort/return, or report a blocked sight-avoid failure.
- Movement must use ordinary movement/pathing/reposition state, not coordinate teleportation.
- Repeated exposure raises `target_alert` and should bias later decisions toward hold/re-scout/stalk rather than immediate raid.

## Success state

- [ ] Every bandit camp owns an independent bandit-side intelligence map, not merely loose shared/global remembered fields or a single current mark.
- [ ] The camp map is serialized/deserialized with the normal game save path, and deterministic save/load coverage proves multiple camps keep distinct map leads across round trip.
- [ ] Scout-return writeback stores a remembered target lead on the source camp map with bounty, threat, confidence, age/last-seen, source/outcome, and target-alert/scout-seen state.
- [x] A vanished live signal does not erase a scout-confirmed camp/basecamp target; later dispatch cadence can plan from remembered scout knowledge. Feature-path proof: `bandit.camp_map_vanished_signal_redispatch` run `.userdata/dev-harness/harness_runs/20260428_185947/`.
- [ ] The remembered-lead and risk/reward decision are wired into the real game path: persisted per-camp map state, scout-return writeback, live dispatch-cadence evaluation, selected member state changes, sight-avoidance state, and reviewer-readable reports/logs. _(Partial feature-path proof now covers vanished-signal redispatch, sight-avoid, 2/5/10 variable-roster sizing, no-opening stalk-pressure, high-threat/low-reward hold, and active-outside dogpile-block guardrails; broader product closure remains.)_
- [ ] Ordinary camp/basecamp scout stand-off uses a two-OMT observation envelope, not the old five-OMT default, with fallback distances reported when terrain/pathing forces them.
- [ ] Scout-watch duration is bounded to about half an in-game day, then the scout returns home and writes memory unless explicitly interrupted.
- [ ] Dispatch sizing uses current living/ready/home roster, home reserve, wounds/unready state, and active outside groups; the same logic handles tiny, medium, and large camps instead of fixed-size folklore.
- [ ] Job intent is chosen before count, and size is derived from that intent plus risk/reward: scout, re-scout, stalk/pressure, toll/shakedown, raid, hold/stale.
- [x] If the camp does not attack after scout return, remembered high-value/manageable-risk leads can produce a larger-than-scout stalk/pressure dispatch that waits for an opening instead of dogpiling or forgetting. Feature-path no-opening proof: `bandit.stalk_pressure_waits_for_opening` run `.userdata/dev-harness/harness_runs/20260428_195617/` proves only active remembered pressure -> no-opening rejection -> stale/decayed lead, not opening-present escalation.
- [x] Scout/stalker sight-avoidance reacts when seen: deterministic and in-game proof show non-teleport reposition or abort immediately or within at most two local turns, with blocked/no-cover cases reported. Feature-path proof: `bandit.scout_stalker_sight_avoid_live` run `.userdata/dev-harness/harness_runs/20260428_173626/`.
- [ ] Camp pressure / stockpile need affects willingness without overriding hard reserve or risk gates. If detailed stockpile state is not available yet, the implementation names the placeholder and keeps it bounded.
- [ ] Bounty, threat, confidence, distance, lead age, target-alert/scout-seen, prior defender losses, and prior bandit losses all have reviewer-readable effects on the chosen intent/member count.
- [x] High threat alone does not force escalation; deterministic and feature-path coverage prove high-threat/low-reward cases hold instead of sending a larger attack. Feature-path proof: `bandit.high_threat_low_reward_holds` run `.userdata/dev-harness/harness_runs/20260428_200600/`.
- [x] Active outbound/local-contact/stalk/returning groups block parallel same-camp same-target dogpile dispatch until resolved. Feature-path proof: `bandit.active_outside_dogpile_block_live` run `.userdata/dev-harness/harness_runs/20260428_200434/`.
- [ ] Reports/logs show remembered-lead source, reward/risk inputs, selected intent/job, selected member count, home reserve left behind, scout/stalk posture, sight-exposure turn count, opening state, and whether a live signal or remembered camp-map lead drove the decision.
- [ ] Harness/product proof covers a real or fixture-backed variable-roster camp through the live game path: scout observes a camp from two OMT, watches for the half-day window, avoids sight by moving/aborting within at most two visible turns if exposed, returns home, writes memory, the live signal disappears or is absent, and a later cadence re-dispatches/plans from remembered camp-map knowledge with expected intent/member count.

## Test plan

### Deterministic tests

- Independent per-camp intelligence maps survive normal save/load, with multiple camps keeping distinct leads and no cross-camp bleed.
- Roster/reserve table: 2, 4, 5, 7, and 10 living-member camps produce expected reserve, dispatchable count, and allowed intents.
- Killed or wounded bandits reduce future dispatch capacity and can increase caution/reserve.
- Active same-target outside/contact/stalk/returning group blocks parallel same-target dispatch.
- Ordinary scout placement chooses two-OMT camp/basecamp observation stand-off, reports fallback if exact placement is impossible, and no longer chooses five OMT as the default.
- Scout-watch sortie clock lasts a named half-day window and returns home/writebacks memory after timeout.
- Remembered camp-map lead survives vanished live signal and remains eligible until stale/invalidated/exploited.
- Five-bandit camp with high bounty, manageable threat, good confidence, and sufficient stockpile pressure chooses more than a one-scout response while preserving home reserve.
- High threat with poor/uncertain reward holds or sends only reconnaissance.
- Low stockpile / high need increases willingness but cannot violate home reserve or hard risk gates.
- Prior defender losses can raise the next bounded pressure/demand; prior bandit losses cool or shrink the next dispatch.
- Returned scout plus high-value/manageable-risk remembered lead can choose a larger stalk/pressure dispatch that waits for opening; high-threat/low-reward still holds or re-scouts.
- No-opening stalk window returns/holds/decays instead of hovering forever.
- Sight-avoid deterministic coverage proves visible/exposed scouts or stalkers take a non-teleport reposition/abort branch by turn two, and separately proves the honest blocked/no-cover report.

### Live wiring proof

Before the harness/product proof can close, show the code bridge explicitly:

- deterministic evaluator coverage proves roster/reserve/risk/reward choice;
- serialization coverage proves independent per-camp maps, remembered leads, and target-alert/scout-seen state survive save/load;
- unit or focused integration coverage proves scout-return writes the remembered lead into the site record;
- source/live-path evidence names the actual dispatch-cadence hook that consumes remembered leads;
- source/live-path evidence names the local sight-avoidance hook that consumes visibility/exposure state;
- report/log fields expose selected intent/job, selected member count, reserve, dispatchable count, scout stand-off distance, watch clock, opening state, sight-exposure turn count, and whether memory, not a fresh signal, drove the decision.

### Harness/product proof

Use named scenarios, not loose manual suggestions. The detailed product matrix lives at `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`; the core risk/reward scenarios are:

1. `bandit.camp_map_scout_memory_two_omt_watch`: known camp, scout reaches two-OMT observation stand-off, watches half-day, returns, writes memory.
2. `bandit.camp_map_vanished_signal_redispatch`: original live signal disappears or is absent; later cadence plans from remembered scout knowledge.
3. `bandit.variable_roster_dispatch_sizing_live`: fixture-backed variable roster proves reserve/dispatch size through live dispatch path.
4. `bandit.stalk_pressure_waits_for_opening`: follow-up stalk/pressure group is larger than scout where justified, waits for opening, and returns/holds if none appears.
5. `bandit.scout_stalker_sight_avoid_live`: exposed scout/stalker repositions or aborts within at most two visible local turns without teleporting.

The broader bandit matrix also covers player-created fire/smoke/light lead proof, shakedown/toll control, empty-camp live sanity, and repeatability/fixture-bias checks.

Every harness step needs the proof-freeze discipline: precondition, action, expected state, screenshot or exact metadata/log proof, failure rule, artifact path, and pass/yellow/red/blocked verdict.

## Andi execution staging

When promoted, Andi should execute in this order:

1. Inspect existing `src/bandit_live_world.*`, dispatch cadence, scout-return writeback, local sight-avoidance, serialization, and report/log seams. Report the actual hook names before coding.
2. Add/extend deterministic helper(s) for roster/reserve/risk/reward intent and size selection. Cover the roster table before live wiring.
3. Add persisted camp-map lead data and save/load coverage.
4. Wire scout-return writeback into the live site record.
5. Change ordinary scout stand-off to two OMT for camp/basecamp observation and add the half-day watch/return clock.
6. Tighten scout/stalker sight-avoidance to reposition/abort within max two visible turns without teleporting.
7. Wire remembered-lead consumption into the real dispatch cadence and selected-member state transitions.
8. Add report/log output.
9. Run deterministic tests/build.
10. Run named harness/product scenarios and classify evidence honestly.

## Classification

Greenlit backlog / ready for promotion. This is now detailed enough for an Andi lane, but `TODO.md` / `andi.handoff.md` should only be switched when Josef/Schani explicitly unleash Andi.
