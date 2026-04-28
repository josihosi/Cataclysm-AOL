# Bandit camp-map risk/reward dispatch planning packet v0 (2026-04-28)

Status: GREENLIT / BOTTOM-OF-STACK

## Why this exists

The current scout-return loop is too disposable if it requires a fresh live signal before the camp can care again. That does not read like bandits. If a scout finds a player/basecamp site, the camp should remember it on its own camp map as a lead with bounty, threat, confidence, age, and source/outcome. The original smoke/light/noise signal can vanish; the scouted camp should still remain a planned target until later evidence decays, confirms, exploits, or invalidates it.

The second missing piece is dispatch sizing. A five-bandit camp should not always send one scout, and it should not automatically send a bigger group just because a threat number is high. It should weigh risk and reward: how many bandits are available, what home reserve is needed, what the camp needs or wants, how valuable the target looks, how dangerous the target looks, how confident the camp is, and what happened on the last contact.

## Scope

1. Add a persistent bandit-side camp-map lead for scout-confirmed targets:
   - target id / approximate overmap position;
   - bounty estimate;
   - threat estimate;
   - confidence;
   - last seen / age / source scout outcome;
   - optional stale/confirmed/invalidated/exploited status.
2. Make scout return write this remembered lead even when the original live signal is gone.
3. Let later dispatch cadence re-plan from remembered scout leads, not only from currently visible live signal packets.
4. Add a bounded risk/reward dispatch-sizing rule using:
   - at-home members and active outside groups;
   - required home reserve;
   - camp stockpile/need/pressure where available, with a simple placeholder if stockpile detail is not yet modeled;
   - target bounty, threat, confidence, distance, and age;
   - recent outcomes such as defender losses, bandit losses, failed extraction, or successful toll.
5. Keep high threat from becoming dumb escalation. High threat can cause hold, more scouting, cautious stalk, or smaller/safer pressure; bigger attack only happens when reward, confidence, available force, and camp pressure justify it.
6. Make reviewer-readable reports show the decision inputs and chosen job/member count.

## Non-goals

- Do not build a global faction war planner.
- Do not make every vanished signal remain actionable forever.
- Do not let high threat alone mean “send more bandits.”
- Do not allow parallel dogpile dispatch from the same camp while an existing active outside group/contact is unresolved.
- Do not close this from deterministic evaluator tests alone if the claim says the live bandit camp re-dispatches from remembered scout knowledge.
- Do not route this as an informal Andi nudge; it is a canon plan item with tests.

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
- [ ] Dispatch sizing uses available at-home members minus home reserve and active outside groups, so a five-bandit camp can choose one scout, a two-bandit toll/stalk group, a larger raid where allowed, or hold.
- [ ] Camp pressure / stockpile need affects willingness without overriding home reserve or risk gates. If detailed stockpile state is not available yet, the implementation names the placeholder and keeps it bounded.
- [ ] Bounty, threat, confidence, distance, lead age, prior defender losses, and prior bandit losses all have reviewer-readable effects on the chosen job/member count.
- [ ] High threat alone does not force escalation; deterministic coverage proves high-threat/low-reward cases hold or scout instead of sending a larger attack.
- [ ] Active outbound/local-contact groups block parallel same-camp dogpile dispatch until resolved.
- [ ] Reports/logs show remembered-lead source, reward/risk inputs, selected job, selected member count, home reserve left behind, and whether a live signal or remembered camp-map lead drove the decision.
- [ ] Harness/product proof covers a real or fixture-backed five-bandit camp: scout observes a camp, returns home, the live signal disappears or is absent, and a later cadence re-dispatches/plans from the remembered camp-map lead with expected member count.

## Test plan

### Deterministic tests

- Five-bandit camp with high bounty, manageable threat, good confidence, and sufficient stockpile pressure chooses more than a one-scout response while preserving home reserve.
- Five-bandit camp with high threat and poor/uncertain reward holds or sends only reconnaissance.
- Five-bandit camp with active scout/outbound/contact member refuses a parallel same-camp dogpile dispatch.
- Low stockpile / high need increases willingness but cannot violate home reserve or hard risk gates.
- Prior defender losses can raise the next bounded pressure/demand; prior bandit losses cool or shrink the next dispatch.
- Remembered camp-map lead survives vanished live signal and remains eligible until stale/invalidated/exploited.

### Harness/product proof

Use a named scenario, not a loose manual suggestion:

1. Start from a known camp with five tracked bandits and explicit stockpile/pressure fixture or placeholder field.
2. Prove initial home roster, home reserve, active group state, and empty/known camp-map memory.
3. Create or observe a player/basecamp target and dispatch a scout.
4. Prove the scout reaches/observes the target and returns home.
5. Prove scout-return writes bounty/threat/confidence into the camp-map lead.
6. Remove or let expire the original live signal.
7. Advance the dispatch cadence.
8. Prove the camp plans/dispatches from remembered scout knowledge, with report fields for job, member count, reserve left behind, risk/reward inputs, and whether the choice was hold/scout/toll/raid.

Every harness step needs the proof-freeze discipline: precondition, action, expected state, screenshot or exact metadata/log proof, failure rule, artifact path, and pass/yellow/red/blocked verdict.

## Classification

Greenlit backlog / bottom-of-stack after the current harness trust audit and already-greenlit actual-playtest validation stack. No Andi handoff packet is produced from this doc by itself; canon should be updated first, and execution should happen only when this item becomes the active lane or is explicitly promoted.
