# Bandit camp-map risk/reward dispatch planning packet v0 (2026-04-28)

Status: GREENLIT / BOTTOM-OF-STACK

## Why this exists

The current scout-return loop is too disposable if it requires a fresh live signal before the camp can care again. That does not read like bandits. If a scout finds a player/basecamp site, the camp should remember it on its own camp map as a lead with bounty, threat, confidence, age, and source/outcome. The original smoke/light/noise signal can vanish; the scouted camp should still remain a planned target until later evidence decays, confirms, exploits, or invalidates it.

The second missing piece is dispatch sizing. A five-bandit camp should not always send one scout, and it should not automatically send a bigger group just because a threat number is high. It should weigh risk and reward: how many bandits are available, what home reserve is needed, what the camp needs or wants, how valuable the target looks, how dangerous the target looks, how confident the camp is, and what happened on the last contact.

The third missing piece is behavior texture. A scout should not sit five overmap tiles away like a nervous weather balloon. For ordinary camp/basecamp observation, the first scout should aim for a two-OMT stand-off from the target OMT, meaning one full OMT remains between scout and camp. The scout watches for about half an in-game day, then returns home and writes the lead. If the camp does not attack immediately, the next pressure beat should usually be a bigger stalk/pressure dispatch that waits for an opening rather than collapsing straight into a fight.

## Scope

1. Add a persistent bandit-side camp-map lead for scout-confirmed targets:
   - target id / approximate overmap position;
   - bounty estimate;
   - threat estimate;
   - confidence;
   - last seen / age / source scout outcome;
   - optional stale/confirmed/invalidated/exploited status.
2. Make scout return write this remembered lead even when the original live signal is gone.
3. Let the real live dispatch cadence re-plan from remembered scout leads, not only from currently visible live signal packets. This must be wired through the actual game path that currently evaluates owned hostile sites, not left as an isolated evaluator helper.
4. Add a bounded risk/reward dispatch-sizing rule using:
   - at-home members and active outside groups;
   - required home reserve;
   - camp stockpile/need/pressure where available, with a simple placeholder if stockpile detail is not yet modeled;
   - target bounty, threat, confidence, distance, and age;
   - recent outcomes such as defender losses, bandit losses, failed extraction, or successful toll.
5. Keep high threat from becoming dumb escalation. High threat can cause hold, more scouting, cautious stalk, or smaller/safer pressure; bigger attack only happens when reward, confidence, available force, and camp pressure justify it.
6. Make reviewer-readable reports show the decision inputs and chosen job/member count.
7. Add the code integration points needed for the live game loop: serialization/deserialization of remembered leads, scout-return writeback, dispatch-cadence consumption, selected-member writeback, and report/log output.
8. Replace the default five-OMT ordinary scout hold-off with a two-OMT scout-watch envelope for camp/basecamp observation, with a named constant or report field for the selected stand-off distance.
9. Give ordinary scout watches a bounded half-day sortie clock, then return home and write memory unless a higher-priority contact/attack/abort condition honestly interrupts it.
10. After a returned scout confirms a target but the camp does not attack, allow the risk/reward cadence to choose a larger stalk/pressure dispatch that waits for an opening, while still preserving reserve, anti-dogpile, and high-threat restraint.
11. Tighten local sight-avoidance for scouts/stalkers: if the player or camp NPCs see the bandit, the bandit must attempt a non-teleport reposition immediately or within at most two local turns, with blocked/no-cover cases reported explicitly.

## Non-goals

- Do not build a global faction war planner.
- Do not make every vanished signal remain actionable forever.
- Do not let high threat alone mean “send more bandits.”
- Do not allow parallel dogpile dispatch from the same camp while an existing active outside group/contact is unresolved.
- Do not keep the previous five-OMT ordinary scout stand-off as the default for camp/basecamp observation; it remains historical proof of the old correction, not the new target behavior.
- Do not make stalk escalation automatic revenge. It is a risk/reward choice after scout memory, with high-threat/low-reward cases still allowed to hold or re-scout.
- Do not close this from deterministic evaluator tests alone if the claim says the live bandit camp re-dispatches from remembered scout knowledge. The live game path must consume the new memory and produce an in-game/harness-observable result.
- Do not route this as an informal Andi nudge; it is a canon plan item with tests.

## Vielleicht scenario pass

Use these scenarios to shape the deterministic logic, not as vague flavor text:

1. **Careful thieves, good target:** five bandits, healthy roster, low-to-moderate threat, high bounty/stockpile need. One scout watches from two OMT for roughly half a day, returns, writes memory, and the next cadence chooses a two-member stalk/pressure group while leaving a home reserve.
2. **Too dangerous, not worth it:** five bandits, high threat, poor reward, low confidence. Scout may observe and return, but the camp holds, re-scouts, or marks stale instead of escalating.
3. **Seen scout, living world:** the player or camp NPC spots the scout/stalker. The bandit moves, not teleports, toward cover/broken line of sight immediately or by turn two; repeated exposure can force abort/return.
4. **Opening appears:** remembered target is valuable, threat is acceptable, and later observations suggest weak guard/time/weather/open camp behavior. The stalk/pressure group may advance to toll/shakedown/raid where the existing job model allows it.
5. **No opening appears:** stalkers wait within their bounded envelope, decay confidence or return home, and do not hover forever.
6. **Camp is stretched thin:** low at-home count, active outside group, wounds, or required reserve blocks bigger dispatch even if the target looks juicy.

## Deterministic behavior sketch

For ordinary camp/basecamp observation:

- Initial scout stand-off target is distance `2 OMT` from the target OMT, so one OMT sits between the scout OMT and camp/basecamp OMT. If terrain/pathing cannot place that exactly, reports must name the fallback distance and reason.
- Scout-watch duration is a named half-day window, nominally `720` in-game minutes, with explicit interruption reasons for attack, contact, exposure, route failure, or abort.
- Scout return writes a remembered lead with bounty/threat/confidence/source/outcome and age.
- Later cadence reads that remembered lead and chooses among hold, re-scout, stalk/pressure, toll/shakedown, or raid from risk/reward.
- A stalk/pressure dispatch from a five-bandit camp should normally be larger than the one-scout probe when reward/confidence justify it, e.g. two members while preserving reserve; it must not become a full-camp dogpile.
- Local sight-avoid records consecutive visible/exposed turns. At `0-1` turns it should try to move immediately if a legal better tile exists; by `2` turns it must either have moved, be mid-abort/return, or report a blocked sight-avoid failure.
- Sight-avoid movement must be ordinary movement/pathing/reposition state, not coordinate teleportation.

## Proposed risk/reward sketch

For a five-bandit camp:

- Keep a home reserve first, e.g. one or two members depending on camp profile and recent losses.
- Compute available dispatch members from at-home members minus reserve and any active outside group.
- Score reward from remembered bounty, confidence, camp need/stockpile pressure, and distance.
- Score risk from remembered threat, uncertainty, previous bandit losses, defender count, zombies/fire/weather, and distance burden.
- Choose a job and size from the resulting margin:
  - low confidence or unclear target: one scout / stalk / hold;
  - high reward and manageable risk: two-member toll or pressure group;
  - high reward, low-to-moderate risk, high camp pressure, enough reserves: larger raid if the current job model allows it;
  - high risk with poor reward: hold, re-scout, or abandon/mark stale;
  - prior defender losses may allow one stronger follow-up demand, while prior bandit losses cool or shrink pressure.

## Success state

- [ ] Scout-return writeback stores a remembered target lead on the source camp map with bounty, threat, confidence, age/last-seen, and source/outcome fields.
- [ ] A vanished live signal does not erase a scout-confirmed camp/basecamp target; later dispatch cadence can plan from remembered scout knowledge.
- [ ] The remembered-lead and risk/reward decision are wired into the real game path: persisted site state, scout-return writeback, live dispatch-cadence evaluation, selected member state changes, and reviewer-readable reports/logs.
- [ ] Ordinary camp/basecamp scout stand-off uses a two-OMT observation envelope, not the old five-OMT default, with fallback distances reported when terrain/pathing forces them.
- [ ] Scout-watch duration is bounded to about half an in-game day, then the scout returns home and writes memory unless explicitly interrupted.
- [ ] Dispatch sizing uses available at-home members minus home reserve and active outside groups, so a five-bandit camp can choose one scout, a two-bandit stalk/toll/pressure group, a larger raid where allowed, or hold.
- [ ] If the camp does not attack after scout return, remembered high-value/manageable-risk leads can produce a larger stalk/pressure dispatch that waits for an opening instead of dogpiling or forgetting.
- [ ] Scout/stalker sight-avoidance reacts when seen: deterministic and in-game proof show non-teleport reposition or abort immediately or within at most two local turns, with blocked/no-cover cases reported.
- [ ] Camp pressure / stockpile need affects willingness without overriding home reserve or risk gates. If detailed stockpile state is not available yet, the implementation names the placeholder and keeps it bounded.
- [ ] Bounty, threat, confidence, distance, lead age, prior defender losses, and prior bandit losses all have reviewer-readable effects on the chosen job/member count.
- [ ] High threat alone does not force escalation; deterministic coverage proves high-threat/low-reward cases hold or scout instead of sending a larger attack.
- [ ] Active outbound/local-contact groups block parallel same-camp dogpile dispatch until resolved.
- [ ] Reports/logs show remembered-lead source, reward/risk inputs, selected job, selected member count, home reserve left behind, and whether a live signal or remembered camp-map lead drove the decision.
- [ ] Harness/product proof covers a real or fixture-backed five-bandit camp: scout observes a camp, returns home, the live signal disappears or is absent, and a later cadence re-dispatches/plans from the remembered camp-map lead with expected member count.

## Test plan

### Deterministic tests

- Ordinary scout placement chooses two-OMT camp/basecamp observation stand-off, reports fallback if exact placement is impossible, and no longer chooses five OMT as the default.
- Scout-watch sortie clock lasts a named half-day window and returns home/writebacks memory after timeout.
- Five-bandit camp with high bounty, manageable threat, good confidence, and sufficient stockpile pressure chooses more than a one-scout response while preserving home reserve.
- Five-bandit camp with high threat and poor/uncertain reward holds or sends only reconnaissance.
- Five-bandit camp with active scout/outbound/contact member refuses a parallel same-camp dogpile dispatch.
- Low stockpile / high need increases willingness but cannot violate home reserve or hard risk gates.
- Prior defender losses can raise the next bounded pressure/demand; prior bandit losses cool or shrink the next dispatch.
- Remembered camp-map lead survives vanished live signal and remains eligible until stale/invalidated/exploited.
- Returned scout plus high-value/manageable-risk remembered lead can choose a larger stalk/pressure dispatch that waits for opening; high-threat/low-reward still holds or re-scouts.
- Sight-avoid deterministic coverage proves visible/exposed scouts or stalkers take a non-teleport reposition/abort branch by turn two, and separately proves the honest blocked/no-cover report.

### Live wiring proof

Before the harness/product proof can close, show the code bridge explicitly:

- deterministic evaluator coverage proves the risk/reward choice;
- serialization coverage proves remembered leads survive save/load;
- unit or focused integration coverage proves scout-return writes the remembered lead into the site record;
- source/live-path evidence names the actual dispatch-cadence hook that consumes remembered leads;
- report/log fields expose the selected job/member count, scout stand-off distance, watch clock, sight-exposure turn count, and whether memory, not a fresh signal, drove the decision.

### Harness/product proof

Use a named scenario, not a loose manual suggestion:

1. Start from a known camp with five tracked bandits and explicit stockpile/pressure fixture or placeholder field.
2. Prove initial home roster, home reserve, active group state, and empty/known camp-map memory.
3. Create or observe a player/basecamp target and dispatch a scout.
4. Prove the scout reaches a two-OMT observation stand-off, watches for the half-day window, avoids sight by moving/aborting within at most two visible turns if exposed, and returns home.
5. Prove scout-return writes bounty/threat/confidence into the camp-map lead.
6. Remove or let expire the original live signal.
7. Advance the dispatch cadence.
8. Prove the camp plans/dispatches from remembered scout knowledge, with report fields for job, member count, reserve left behind, risk/reward inputs, scout/stalk posture, and whether the choice was hold/re-scout/stalk/toll/raid.
9. If the chosen follow-up is stalk/pressure, prove it uses a larger-than-scout dispatch where justified, waits for an opening instead of instant dogpile, and still retreats/returns/holds when no opening appears.

Every harness step needs the proof-freeze discipline: precondition, action, expected state, screenshot or exact metadata/log proof, failure rule, artifact path, and pass/yellow/red/blocked verdict.

## Classification

Greenlit backlog / bottom-of-stack after the current harness trust audit and already-greenlit actual-playtest validation stack. No Andi handoff packet is produced from this doc by itself; canon should be updated first, and execution should happen only when this item becomes the active lane or is explicitly promoted.
