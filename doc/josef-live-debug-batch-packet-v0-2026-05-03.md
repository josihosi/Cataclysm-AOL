# CAOL-JOSEF-LIVE-DEBUG-BATCH-v0 — 2026-05-03

Status: ACTIVE / GREENLIT DEBUG-PACKET STACK

Imagination source: `doc/josef-live-debug-batch-imagination-source-2026-05-03.md`.

Test matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`.

Raw intake:
- `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`
- `runtime/josef-bandit-debug-intake-2026-05-03.md`
- `runtime/josef-locker-zone-debug-intake-2026-05-03.md`

## Why this exists

Josef finished a two-day live debug pass and asked to package the good notes into canon. Several issues were not new: the shakedown two-choice/trade-debt notes existed in 2026-05-02 raw intake, then later repo canon and harness proof misleadingly blessed the wrong `Pay / Fight / Refuse` surface.

This packet turns the raw notes into an ordered execution stack without pretending every note is the same subsystem. The main stack is bandit/live-world heavy, but Josef also left locker/basecamp equipment debug notes in the same session; those are preserved as a bottom slice instead of being dropped under the rug. Wieder so ein Papierkorb mit Zähnen.

## Classification

Active debug-packet stack. Execute in order unless a slice exposes a genuine blocker or Schani/Josef reprioritizes. Where a behavior is profile-neutral — standoff distance, sight avoidance, hot-tile retreat, dispatch-size reasoning, proof helpers — transpose it to other hostile profiles instead of baking the fix as bandit-only. Preserve profile-specific outcomes: bandits may shakedown/toll; cannibals stalk and attack, not negotiate.

## Slice order

### 1. Shakedown UI/payment contract correction

Fix the shakedown surface and the stranded-note canon failure.

Scope:
- visible choices are exactly **Pay** and **Fight**;
- Esc/backout/refuse routes to the fight/refusal branch but is not shown as a third response;
- pay opens the actual NPC trade UI / trade window, i.e. the same kind of window opened by normal NPC trading (`talk_function::start_trade` / `npc_trading::trade` / `trade_ui`), not a custom prompt or silent selector;
- the trade window autostarts with a demanded debt/toll balance (`Pay:`-style owed/cost) and the player must dump enough items into the offer side to satisfy it;
- it must not silently call `live_bandit_surrender_goods()` before the player sees and uses that window;
- the payment window is backed by the whole honest basecamp/faction-side pool: carried goods, basecamp goods, basecamp NPC carried items, and nearby honest faction/basecamp inventory;
- the demanded debt/toll is derived from what that camp side has / can plausibly pay, not a fixed/stub amount and not only the player's carried inventory;
- preserve reopened higher-demand behavior and cannibal/no-shakedown separation;
- correct docs/tests/log expectations that currently bless `responses=pay/fight/refuse` as success.

Non-goals:
- no full diplomacy tree;
- no remote magical inventory outside the honest scene/faction pool;
- no reputation/hostage/economy redesign.

### 2. Defended-camp scout standoff, sight avoidance, and hot-tile looting

Fix the doorstep scout/flee/loot problem.

Scope:
- restore defended camp/house scout/hold-off watch distance to about `5` OMT where pathing permits;
- transpose this standoff discipline to cannibals and other stalking hostile profiles where the same posture semantics apply;
- apply the same real sight-avoidance expectation to bandits, cannibals, and other stalking-mode hostiles: if the player/camp is actually looking at them, they back off, break LoS, reroute through cover, or escalate/report instead of staying visible with a `stalk` label;
- sighted scout/hold-off actors near a defended camp mark the tile hot and back off/reroute, not only adjacent-step in `local_contact` footing;
- a fleeing bandit keeps fleeing, breaks LoS, or logs a concrete blocker instead of standing beside camp;
- generic item pickup/collect-bounty behavior must not continue on a hot defended doorstep.

Non-goals:
- no blanket ban on bandits collecting useful loose loot;
- no suicidal rush from every scout.

### 3. Multi-z camp identity, roof-z routing, and dispatch retry throttle

Fix the roof-fire response stall.

Scope:
- collapse multi-z `bandit_camp` placement into one owner/site keyed by overmap-special placement x/y, with z-level footprint metadata;
- dispatch routes to reachable ground/approach/standoff OMT first; local z-transition/roof/stair behavior is handled later in the local/reality-bubble layer;
- `route_missing` against a roof-z target must not leave `active_sites=0`, `travelling_npcs=0`, and `30_minute_throttle` silence;
- preserve the current real fire/horde signal footing.

Non-goals:
- no full vertical building assault AI;
- no broad overmap pathfinder rewrite beyond the dispatch target/fallback seam.

### 4. Hostile-camp escalation after scout confirmation

Fix the lone-scout loop and transpose the usable dispatch reasoning to other hostile camps.

Scope:
- after a scout returns with confirmed/basecamp contact and `remaining_pressure=ample`, a sufficiently staffed ordinary bandit camp can promote the next response to `toll`/shakedown-capable pressure;
- cannibal camps should also stalk from about `5` OMT, then if threat math says they can overpower the defenders, send an attack dispatch instead of another timid scout;
- cannibal attack dispatch size can scale up to a large/whole-camp commitment when odds, distance, roster, and home-reserve policy justify it;
- shared dispatch sizing should scale from available roster while preserving any required home reserve/profile caution;
- local gate can open bandit shakedown for the larger party instead of repeating `strength=1 margin=0` hold-off; cannibal local gate enters attack/contact pressure, not toll UI;
- weak/small/high-threat camps stay cautious.

Non-goals:
- no every-camp suicide dogpile;
- no cannibal shakedown/diplomacy;
- no unrelated faction economy rewrite.

### 5. All-light-source live adapter and horde/bandit interpretation

Fix the fire-only narrowing.

Scope:
- ordinary meaningful exposed light sources — lamps, household/window light, searchlights, bright appliances, fire — can feed the live light-signal adapter when brightness/weather/time/exposure justify it;
- bandits distinguish searchlight/directional active-defense threat from household/fire occupancy/bounty evidence;
- zombies/hordes do not distinguish source type that finely: exposed visible light is attraction pressure;
- keep daylight, weak-light, containment, weather, and range suppression bounded;
- audit the weather source mismatch seen during intake, where Josef reported fog but the live adapter logged `light_weather=clear`;
- keep existing `fd_fire` roof-fire proof green;
- clarify/debug the spawned-horde/no-horde-symbol tooling gap separately from actual horde deletion;
- investigate stale-looking horde destinations where saved horde entries keep destination `[3360,984,1]` while `tracking=0`, and either expire/clear them correctly or document why that state is intentional.

Non-goals:
- no tile-perfect second light engine in overmap code;
- no guarantee that every decorative light attracts across arbitrary distance.

### 6. Camp patrol aggression, alarm state, and patrol-report hygiene

Fix passive patrols and noisy reports.

Scope:
- camp patrol NPCs attack hostile bandits/zombies on sight;
- neutrals/unknown travelers are not false-positive attacked;
- hostile sighting can put the whole camp into alarm state, pulling every NPC with patrol responsibility onto the patrol roster independent of shift;
- routine patrol route/assignment reports stay in debug/artifacts or behind an explicit verbose setting; visible messages are reserved for actionable exceptions.

Non-goals:
- no full military command simulation;
- no hostile classification against allies/neutrals by vibe.

### 7. Writhing stalker distance, sight avoidance, and threat-drop rhythm

Refine the closed threat/distraction packet from Josef's live feel note and the later correction that the stalker is still too close/visible/passive.

Scope:
- high threat still causes stalking/withdraw/cover, but stalking distance should be bigger than the prior close v0 footing; tune toward a believable far watch distance, not a visible doorstep/window loiter;
- sight avoidance must actually happen: if the player/defenders have clear LoS to the stalker during stalking/withdraw, it should break LoS, move through cover/darkness, increase range, or log a concrete blocker;
- when local threat drops through zombie distraction, defender death, line-pressure loss, or player vulnerability, the stalker quickly converts to approach/pounce/short strike pressure;
- zombie engagement/threat reduction creates target/opportunity footing even if `mon_plan.target` was previously missing;
- after the strike/burst, the stalker quickly boots itself back out into cover/stalking/cooldown instead of remaining exposed or idling near the victim;
- when threat rises again, the stalker exits/re-covers instead of standing exposed.

Non-goals:
- no omniscient backstab magic;
- no suicidal charge through high-threat exposure.

### 8. NPC sorting failure debounce

External debug note, not from the bandit playtest, but packaged because it affects basecamp stability.

Scope:
- failed NPC sorting attempts record a job-level/source-level cooldown or blocked state;
- repeated job scans do not recreate `ACT_MOVE_LOOT` every turn against the same impossible setup;
- retry is allowed after cooldown, explicit retry, zone/inventory/path state change, or fixed destination;
- visible/debug log spam is bounded;
- successful ordinary sorting remains unchanged.

Non-goals:
- no broad sorting redesign;
- no removal of NPC sorting.

### 9. Debug spawn horde / stalker / rider options

Follow-up debug-helper request from Josef. This is tooling for staging the same live problems without rebuilding saves by hand.

Scope:
- `spawn horde` should create a medium-sized horde that is large enough to test pressure/attraction, not a tiny or empty token;
- add explicit horde distance options, including `5` OMT and `10` OMT where the existing UI/helper shape permits;
- add writhing stalker spawn options at `5` OMT and `10` OMT;
- add zombie rider spawn options at `5` OMT and `10` OMT;
- option labels should make size/distance clear enough that Josef can pick the intended setup without reading debug logs;
- proof must distinguish spawned horde bookkeeping from visible/local monster spawn where relevant, so the old no-horde-symbol confusion does not repeat.

Non-goals:
- no broad debug-menu redesign;
- no balance claim about natural horde, stalker, or rider spawning;
- no requirement that every spawned distant entity immediately enters the reality bubble.

### 10. Locker/basecamp equipment consistency follow-up

Session-memory cleanup item from Josef's locker-zone debug note. This belongs at the bottom of the stack so the note is not lost, but it should not interrupt shakedown/bandit execution.

Scope:
- NPC ammo/magazine pickup should require a compatible carried/kept firearm, unless an explicit camp/storage policy says the item is being hauled for storage rather than personal loadout;
- if an NPC drops/replaces the firearm that justified carried ammo/magazines, orphan ammo/magazines should be dropped/returned too, unless still compatible with another carried/kept weapon;
- clothing/backpack rearrangement should handle items stored inside worn containers when replacing a broken/obsolete container;
- a broken/`XX` backpack should not be kept forever merely because it contains items if a better backpack and safe transfer/storage capacity exist;
- if item transfer fails, record a diagnosable reason rather than silently preserving the broken container.

Non-goals:
- no whole wardrobe/encumbrance redesign;
- no broad loot valuation rewrite;
- no forced dropping of ammo/magazines that still match another carried/kept weapon or explicit camp hauling task.

### 11. Cannibal Monsterbone spear lore item

Follow-up lore/equipment request from Josef. This should be a small item/distribution slice, not a balance flood.

Scope:
- add a new item named `Monsterbone spear` / `monsterbone spear` for cannibals;
- make it a fairly strong elite spear, sturdier than ordinary improvised wood/bone, but not overpowered against proper late-game weapons;
- make it ritualistic/status-coded rather than an everyday cannibal weapon;
- its description should carry the lore: it is made from a huge monster bone, implying the cannibals ate monster meat from something vast enough to be a dungeon/brain-world and were changed by it;
- place rare copies in cannibal camps, roughly `1` or `2` per camp where camp item-group placement supports it;
- allow only a couple important cannibal NPCs to wield it, biased toward leaders/champions/elite hunters or an explicit elite cannibal weapon group rather than every cannibal;
- preserve cannibal no-shakedown profile and current camp identity; this is flavor/equipment plus combat texture.

Non-goals:
- no common loot flood;
- no OP artifact spear;
- no common/everybody weapon distribution;
- no full monsterbone crafting economy unless later promoted;
- no broad cannibal lore rewrite beyond this readable prop.

## Known non-bugs / de-escalated notes

- Horde deletion after the roof fight looks okay in the audited save: `total_horde_entities` and nearby counts dropped, and no killed local empty horde bucket remained.
- Remaining distant entries targeting `[3360,984,1]` with `tracking=0` are a stale-looking destination/state cleanup or observability follow-up under Slice 5, not current proof of killed hordes being reinserted.
- Deleted patrol-zone suspicion from 2026-05-02 was de-escalated by Josef; focus patrol work on log noise/aggression/alarm unless fresh evidence reopens stale zone assignment.

## Success state

- [x] Shakedown visible UI is two-choice Pay/Fight; backout/refuse enters fight/refusal without a third visible response. _Red pre-repair row: `20260503_173812`; final credited UI-first row after Josef's no-credit nudge is tight cropped-OCR proof `20260503_202326` on rebuilt `d1a4f076c8`, showing pre-selection `Pay` and `Fight` only._
- [x] Pay opens the actual NPC trade UI / trade window with an initial demanded debt/toll balance and the honest basecamp/faction-side pool before any silent surrender; the payment pool and demanded debt/toll basis are proven non-stub/non-player-carried-only; successful payment saves/persists the paid writeback. _Proof: `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`; final tight row `20260503_202326` proves immediate post-Pay `Pay:` / `Debt $157.97` / `F1 to auto balance` plus `npc_trading::trade` artifacts and pool `3211 + 5222 + 36701 = 45134`; supporting reopened/cancel/save rows: `20260503_192442`, `20260503_192911`, `20260503_193524`; deterministic shakedown gate `/tmp/caol_shakedown_contract_tests_20260503.log`._
- [x] Misleading `pay/fight/refuse` docs/tests/log expectations are corrected.
- [ ] Defended camp scout/hold-off uses about `5` OMT watch distance; bandits, cannibals, and other compatible stalking-mode hostiles perform real sight avoidance, and sighted/hot doorstep behavior backs off or escalates. _Partial checkpoint: `doc/defended-camp-scout-standoff-hot-loot-proof-v0-2026-05-03.md` credits deterministic/local-gate + live artifact evidence for the `5` OMT hold-off goal and hot-doorstep pickup guard; still missing full live sight/smoke break-LoS/reroute/wait/escalate proof._
- [x] Multi-z bandit camp is one site/owner with z-footprint metadata; roof-z/tower-z player target does not create route-missing/throttle silence. _Proof: `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md`; deterministic `[multi_z]` + targeted vertical route fallback tests, and live `bandit.roof_z_dispatch_fallback_mcw` run `.userdata/dev-harness/harness_runs/20260503_201355/` with elevated target id `player@140,39,5`, ground `live_dispatch_goal=140,39,0`, saved active stalk dispatch, and negative `route_missing` / empty-throttle guard._
- [ ] Confirmed big-camp contact can escalate from lone scout into roster-scaled pressure when appropriate: bandits into toll/shakedown pressure, cannibals into attack dispatch up to large/whole-camp commitment when they can overpower the defenders.
- [ ] Lamps/household/searchlight/fire sources can feed bounded live light signals; zombies receive broad light attraction, bandits preserve light-type semantics; weather state used by the adapter matches live conditions or logs the mismatch, and stale `tracking=0` horde destinations are handled or explained.
- [ ] Patrols attack hostile bandits/zombies on sight, avoid neutral false positives, and can trigger camp alarm roster behavior.
- [ ] Routine patrol assignment/report spam is no longer visible during waits.
- [ ] Writhing stalker uses larger stalking distance, performs real sight avoidance, quickly swoops when threat drops, and quickly boots back out/recovers when threat rises or after its strike burst.
- [ ] NPC sorting failures are debounced; failing sort jobs do not retry/log every turn.
- [ ] Debug spawn options can create a medium horde, horde-at-`5`/`10` OMT setups, writhing stalker at `5`/`10` OMT, and zombie rider at `5`/`10` OMT, with clear labels and honest spawn/location proof.
- [ ] Locker/basecamp equipment cleanup prevents orphan ammo/magazine carry after firearm replacement and can replace broken/`XX` backpacks by transferring stored contents or logging a concrete transfer blocker.
- [ ] Cannibal camps and selected important cannibal NPC loadouts can surface a rare ritual/status `Monsterbone spear` that reads as monster-meat/huge-bone lore, with only about `1` or `2` camp copies and bounded elite wielder frequency.
- [ ] Each slice has deterministic coverage and at least one appropriate live/path proof where the product claim is live behavior.

## Testing impact

This packet changes validation obligations. Full scenario matrix: `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`. Each slice needs the smallest honest gate:

- shakedown: deterministic response-count/payment-bridge tests plus live dialogue/trade proof for first and reopened demand;
- scout/standoff/routing/escalation: deterministic planner/local-gate tests plus live multi-turn bandit and cannibal rows around defended base, stalking-mode LoS avoidance, cannibal `5` OMT stalk-to-attack escalation, and roof-z routing;
- light adapter: deterministic light-source classification tests plus live lamp/household/fire controls at night/day/weather, weather-source sanity, and horde destination/tracking save inspection;
- patrol: focused camp patrol hostile/neutral/alarm/log-hygiene tests plus live/log wait proof;
- stalker: deterministic larger-distance/LoS-avoidance/threat-drop transition tests plus staged/live pounce-and-boot-out timing row;
- sorting: deterministic failing-sort debounce/recovery tests plus bounded wait/log proof if needed;
- debug spawn options: deterministic option/menu coverage plus live/save inspection proving medium horde size and requested `5`/`10` OMT horde/stalker/rider placement/state;
- locker/basecamp equipment: deterministic ammo/firearm compatibility and container-content transfer tests, plus a narrow live/fixture proof if the bug only reproduces through the locker-zone/basecamp flow;
- Monsterbone spear: JSON validation plus item stat sanity, camp item-group distribution check for rare `1`-or-`2` copies, and NPC weapon-group/loadout proof that only selected important/elite cannibals can wield it.

## Handoff caution

Do not close any slice from a test seam alone when the claim is live UI or live behavior. The proof must reach the actual player-facing surface, live dispatch/local gate, real signal adapter, or real activity/job loop named by the slice.
