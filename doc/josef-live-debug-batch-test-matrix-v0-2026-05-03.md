# CAOL-JOSEF-LIVE-DEBUG-BATCH-v0 test matrix — 2026-05-03

Status: ACTIVE TEST DESIGN / AUXILIARY TO `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`

Contract: `doc/josef-live-debug-batch-packet-v0-2026-05-03.md`.

This matrix is the validation shape for the debug batch. It is deliberately split by evidence class so deterministic switches do not impersonate live product proof and live playtests do not replace cheap contract tests.

## Evidence policy

Use three layers:

1. **Deterministic internal switch tests** for decisions, thresholds, profile splits, and state transitions.
2. **Staged/live harness rows** for actual game-path proof: dispatch, local gate, monster plan, dialogue UI, light/horde adapter, save/writeback.
3. **Josef playtests** only for feel/readability/fairness where agent-side proof cannot answer the product question.

Each row should name:
- actor/profile: bandit, cannibal, writhing stalker, zombie rider, horde, basecamp NPC;
- player footing: ground/outside, inside ground floor, first floor/upstairs/roof (`z=1`), darkness/light, defended camp, open field;
- threat state: low, equal, high, threat drop, threat rise;
- expected decision and cadence;
- evidence class required to close the row.

## Cross-cutting deterministic tests

### Shared hostile standoff / sight-avoidance switch

Deterministic rows:
- **No sight / low threat:** stalking actor holds watch/stalk state; no forced retreat.
- **Player LoS current:** stalking actor marks tile hot and chooses break-LoS/backoff/reroute/escalate.
- **Camp NPC LoS current:** same as player LoS; use unfriendly camp NPC perspective, not only avatar vision.
- **Recent LoS only:** more cautious posture / route adjustment, but not necessarily immediate full retreat if already hidden.
- **Cover exists:** choose cover/darkness/blocked-sight tile over raw distance when both are available.
- **No cover exists:** increase range / leave OMT / go home / log concrete blocker instead of visible idle.
- **Defended camp/basecamp:** minimum watch distance about `5` OMT for compatible scout/stalk/hold-off profiles.
- **Profile split:** bandits and cannibals share standoff/sight-avoid primitives where applicable; profile outcome remains different.

Expected deterministic seam: planner/local-gate/evaluator tests can assert distance target, LoS flag handling, hot-tile flag, selected posture, and fallback reason.

### Vertical / first-floor / roof / tower target handling

Deterministic rows:
- player inside ground floor (`z=0`) -> overmap dispatch targets reachable ground approach/standoff OMT;
- player upstairs / first floor / roof (`z=1`) -> overmap dispatch still targets reachable ground approach/standoff OMT, then local/bubble layer handles stairs/roof/vertical approach;
- player in tower / very high camp (`z=5`) -> overmap/site identity still collapses to one player camp/site with z-footprint metadata; hostile dispatch targets reachable ground approach/standoff OMT and local layer treats vertical access, sightlines, and blocker reasons explicitly;
- no known route to target z -> fallback to reachable approach target, not `route_missing` plus long throttle silence;
- multi-z camp special -> one owner/site with z-footprint metadata, not duplicated camps.

Live rows must include at least one roof/upstairs setup, one ordinary ground-floor/inside setup, and one high-z/tower (`z=5` or closest supported fixture) setup.

Current roof/tower-z dispatch fallback checkpoint: `doc/multi-z-roof-dispatch-fallback-proof-v0-2026-05-03.md` credits deterministic multi-z identity/routing and live `bandit.roof_z_dispatch_fallback_mcw` proof for preserving elevated target id while routing the dispatch goal through reachable ground and avoiding the old `route_missing` / empty-throttle silence. Broader local/reality-bubble roof/stair entry remains a separate future/local-layer claim.

### Player counter-signal / smoke-out behavior

Deterministic rows:
- player smoke overlaps suspected stalking location -> stalker/scout confidence drops or mark changes from clear visual lead to obscured/uncertain lead;
- smoke between player camp and hostile watcher -> LoS/exposure becomes blocked/uncertain, not magically transparent;
- smoke at hostile watcher tile -> compatible bandit/cannibal/stalker profiles reposition, wait, probe around, or escalate based on threat/profile rather than standing in the smoke;
- smoke from player camp itself -> zombies/hordes may treat exposed smoke/fire broadly as attraction, while bandits/cannibals treat it as occupancy/concealment/threat context and preserve profile-specific response;
- repeated smoke-out of same stalking spot -> hostile should not retry the exact exposed/obscured tile in a loop without decay/cooldown/reason.

Live rows must include at least one bandit and one cannibal smoke-out case. Writhing stalker smoke-out can be deterministic-first unless a staged local smoke fixture is cheap.

## Bandit rows

### Deterministic

- **Shakedown visible fork:** first-demand and reopened-demand surfaces expose exactly Pay/Fight; Esc/backout/refuse maps to fight/refusal without visible third option.
- **Pay bridge:** pay branch opens the actual NPC trade UI / trade window (`npc_trading::trade` / `trade_ui` shape) with demanded debt/toll already applied; no custom fake selector and no direct hidden `live_bandit_surrender_goods()` as visible pay path.
- **Payment pool:** the whole honest basecamp/faction-side inventory is eligible: carried goods, basecamp goods, basecamp NPC carried items, and honest nearby faction/basecamp inventory; remote magical inventory is not.
- **Debt basis:** demanded debt/toll is derived from the reachable camp-side pool/value and clamps/fails legibly for poor camps; it is not fixed, stubbed, or based only on the avatar's carried items.
- **Scout standoff:** defended base/house watch target is about `5` OMT, not 1-2 OMT doorstep.
- **Sighted stalker/scout:** current LoS from player or hostile camp NPC forces break-LoS/backoff/reroute/escalate; hot tile disables casual casing pickup.
- **Scout return escalation:** confirmed basecamp contact + ample roster promotes next pressure from lone scout to toll/shakedown-capable party while preserving home reserve.
- **Weak/high-threat caution:** weak camp, small roster, or high defender threat stays cautious; no suicide dogpile.
- **Multi-z route fallback:** player at roof/first floor does not produce roof-z overmap route failure.

### Staged/live harness

- **Defended doorstep:** bandit near camp/house and visibly looked at by player/NPCs breaks LoS or backs off; no stationary doorway/casing goblin behavior.
- **Scout returns:** scout observes, returns, camp has ample roster, next dispatch is larger/toll-capable rather than another single scout.
- **High-threat hold:** three armed defenders / camp threat make scout keep binocular distance or abort.
- **Roof/first-floor/tower player:** player upstairs/roof/`z=5` tower with visible fire/light does not split camp or route to unreachable z; bandit dispatch chooses reachable ground approach and records vertical blocker/access if needed.
- **Smoke out watcher:** if the player smokes the suspected stalking location or line between camp and watcher, bandit switches to obscured-lead/reposition/probe/escalate logic instead of holding the same visible tile.
- **Lamp vs searchlight vs fire/smoke:** bandit distinguishes household/fire/smoke occupancy evidence from directional searchlight/active-defense threat.
- **Payment UI:** selecting Pay visibly enters the real NPC trade window, starts with a debt/toll balance, shows/selects from the whole camp-side pool, and success/cancel/insufficient-payment outcomes are distinct. The proof must show the player can dump items into the trade offer to satisfy the debt.

### Josef feel playtests

- Does `5` OMT watch distance read like binocular stalking instead of absence?
- Does a larger shakedown party feel threatening/fair rather than instant nonsense?
- Does Pay/Fight read bluntly enough, without UI fuss?

## Writhing stalker rows

### Deterministic

- **High threat baseline:** player + multiple defenders / bright light -> stalker retreats, holds far stalk distance, or uses cover; no rush.
- **Bigger stalk distance:** stalker target distance is larger than previous close v0 footing and avoids doorstep/window loiter.
- **Sight avoidance:** if player/defenders have clear LoS, stalker breaks LoS, moves through dark/cover, increases range, or logs blocker.
- **Threat drop:** zombies engage defenders, defenders die, line pressure drops, or player vulnerability rises -> bounded quick approach/pounce/short strike.
- **One zombie vs multi-zombie:** one zombie may create partial opportunity; multiple zombies / engaged defenders create stronger pounce footing. Thresholds must be explicit, not vibes.
- **No-target opportunity:** zombie engagement can create target/opportunity footing even if `mon_plan.target` was previously missing.
- **Post-burst boot-out:** after a few strikes or spent opportunity, stalker exits to cover/stalk/cooldown instead of remaining exposed.
- **Threat rise:** zombies die, defender LoS returns, player focuses it -> stalker recovers/withdraws.
- **Player hunts stalker:** if the player closes on / attacks / corners the stalker during stalking, it uses combat fallback and fights back locally until overmap/retreat logic can pull it away; no helpless non-action.

### Staged/live harness

- **Inside ground floor:** player inside house, stalker outside; it should not stand in front of the window if seen.
- **Player first floor/upstairs/roof/tower (`z=1`/`z=5`):** stalker should not overmap-route to impossible z; should watch/approach via reachable local layer, stairs/climb access if supported, or log blocker.
- **Player smokes stalking side:** smoke blocks/obscures the line or suspected hide; stalker shifts hide/approach side or waits instead of freezing in the smoke.
- **Zombie distraction:** zombie(s) enter house / engage defenders; stalker swoops within bounded cadence.
- **After zombie death:** stalker boots back out / resumes stalking instead of hanging in melee range.
- **Player hunts it:** Josef/player exits and pursues or attacks; stalker either fights back briefly, breaks LoS, or retreats with legible reason.
- **Light/camp pressure:** fire/lamp/searchlight plus defenders changes stalker confidence; no omniscient backstab through impossible exposure.

### Josef feel playtests

- Does the stalker feel like a predator choosing windows rather than an idle NPC?
- Is the pounce too slow, too suicidal, or readable/fair?
- Does chasing it produce satisfying danger instead of a bug-looking retreat script?

## Cannibal rows

### Deterministic

- **5 OMT stalk:** cannibal scout/stalk/hold profiles use about `5` OMT standoff where terrain permits.
- **Sight avoidance:** cannibals share compatible hostile sight-avoid primitives; no visible loitering under LoS.
- **No shakedown:** cannibal local gate never opens bandit Pay/Fight toll UI.
- **Overpower decision:** if roster, odds, darkness/concealment, and target value say they can overpower defenders, cannibals promote from stalk/scout into attack dispatch.
- **Whole-camp bound:** large/whole-camp commitment is allowed only when roster/odds/reserve policy justify it; high-threat/weak camps remain cautious.
- **Profile outcome split:** shared standoff/escalation machinery dispatches cannibals to attack/contact pressure, bandits to toll/shakedown pressure.
- **Monsterbone spear distribution:** rare elite/status item, not common; only selected leaders/champions/elite hunters can wield it.

### Staged/live harness

- **Day/high threat:** cannibals stalk/hold from distance; no daylight suicide rush.
- **Night/concealment favorable:** cannibals attack with a multi-member dispatch.
- **Defended base contact:** if they believe they can overpower defenders, dispatch scales up; if not, they watch/withdraw.
- **Player first floor/roof/tower (`z=5`):** cannibal dispatch chooses reachable ground approach; local layer handles verticality or logs blocker.
- **Smoke out stalking location:** player smoke on suspected cannibal watcher/approach line causes reposition/wait/reroute/attack-decision update, not same-tile smoke camping.
- **Monsterbone spear camp:** save/load or fixture audit shows 1-2 camp copies and selected important wielders, not every cannibal.

### Josef feel playtests

- Does cannibal stalking feel like predation rather than bandit bureaucracy?
- Does a large cannibal attack feel scary/fair or too sudden?
- Does Monsterbone spear communicate ritual/lore without becoming loot spam?

## Zombie rider rows

### Deterministic

- **Spawn helper:** rider can be staged at `5` and `10` OMT; saved state proves placement.
- **Cover / close pressure:** if player has cover or enters bad close-pressure band, rider repositions instead of inertly failing ranged logic.
- **Wounded contrast:** wounded/pressured rider behavior remains distinct from healthy confident pressure.
- **Vertical target:** player first floor/roof/inside/`z=5` tower does not make rider choose impossible direct line; it circles, holds, dismount/approach if supported, or logs blocker.
- **Smoke/blocked line:** smoke or building cover should alter ranged pressure/line-of-fire rather than producing inert no-attack deadlock.
- **Player hunts rider:** if player closes aggressively, rider either keeps distance, swaps posture, or fights back; no no-attack deadlock.
- **Light/horde chaos:** rider behavior remains bounded when zombies/hordes/lights are present; no accidental alliance or target confusion.

### Staged/live harness

- rider spawned 5 OMT away, open field: approaches/pressures within expected cadence;
- rider spawned 10 OMT away: pathing/dispatch state persists and does not silently vanish;
- player inside/first floor/roof/`z=5` tower: rider circles/holds/repositions instead of impossible z-target beeline;
- smoke/cover blocks line: rider visibly repositions/holds/swaps posture rather than inertly timing out;
- player hunts rider: combat/reposition fallback is visible.

### Josef feel playtests

- Does rider pressure feel mobile and legible?
- Does hiding upstairs/inside feel like counterplay, not an AI shutdown button?

## Horde / light / debug-spawn rows

### Deterministic

- `spawn horde` default creates a medium horde; not empty/tiny.
- horde/stalker/rider `5` and `10` OMT options resolve to expected overmap offsets.
- fire, lamp, household/window light, searchlight classify into correct signal buckets.
- daylight/weather/fog/exposure suppression is explicit and testable.
- horde destination/tracking state expires or is explained when `tracking=0`.

### Staged/live harness

- medium horde spawn audit: saved overmap metadata proves size/location/destination/tracking.
- 5 OMT horde under exposed night light: horde attraction state changes or records blocker.
- 10 OMT horde under weak/day/fog/contained light: suppression/limited attraction is explicit.
- lamp on roof at night/fog: adapter sees ordinary light or logs why not; weather source matches live visible weather or flags mismatch.
- fire on roof: existing fire proof remains green after adapter broadening.

## Shakedown / UI / economy rows

### Deterministic

- first and reopened shakedown visible choices exactly Pay/Fight;
- Pay opens the actual NPC trade window with demanded toll/debt already active and derived from camp-side value;
- cancel/insufficient payment enters fight/refusal branch without hidden confiscation;
- successful payment records paid/writeback/send-home;
- no-shakedown profiles remain no-shakedown.

### Live

- screenshot/OCR of first demand and reopened demand showing Pay/Fight only;
- screenshot/OCR of the actual NPC trade window showing initial debt/toll and that it is backed by the whole camp-side inventory pool;
- save/log proof of successful payment and cancelled/insufficient payment branch.
- green current checkpoint for visible fork + actual trade-window opening + whole basecamp-side pool + pool-derived debt/toll basis + cancel/refusal + successful saved writeback: `doc/shakedown-pay-fight-npc-trade-ui-proof-v0-2026-05-03.md`; final UI-first closure is tight cropped-OCR run `20260503_202326` on rebuilt `d1a4f076c8` (`Pay`/`Fight` before selection, immediate post-Pay `Pay:` / `Debt $157.97` / `F1 to auto balance`, same-run `npc_trading::trade` artifact and pool components). Supporting rows: reopened Pay/open `20260503_192442`, cancel/refusal `20260503_192911`, current-HEAD successful Pay/save/writeback `20260503_203141`, plus deterministic shakedown gate `/tmp/caol_shakedown_contract_tests_20260503.log`.

## Locker/basecamp equipment rows

### Deterministic

- NPC with rifle + magazine keeps compatible magazine.
- NPC replacing/dropping rifle drops/returns orphan magazine/ammo.
- NPC with two compatible firearms keeps only matching ammo/mags.
- broken/`XX` backpack with contents is replaced by better backpack when transfer capacity exists.
- transfer failure logs concrete blocker and does not loop silently.

### Live/fixture

- locker/basecamp worker fixture reproduces orphan-mag cleanup;
- backpack transfer fixture proves contents move or blocker is logged.

## Minimum playtest packet for Josef

Only after deterministic and staged rows are shaped, prepare a short Josef-facing playtest packet with 4-6 scenes:

1. Bandit defended camp: scout watches from distance, gets seen, backs off, later escalates.
2. Smoke-out hostile watcher: player smokes suspected bandit/cannibal stalking location or approach line and observes reposition/wait/reroute/escalation.
3. High-z camp/tower: player camp on first floor/roof and a high tower (`z=5` if feasible) verifies hostile ground approach + vertical blocker/access behavior.
4. Writhing stalker house: player inside/first floor, zombies distract defenders, stalker swoops, then boots out; second branch where player hunts it.
5. Cannibal camp: night/concealment attack dispatch vs day/high-threat stalk/hold contrast.
6. Zombie rider: 5/10 OMT pressure plus inside/first-floor/tower counterplay.
7. Light/horde debug: medium horde + lamp/fire/searchlight/smoke contrast.
8. Optional Monsterbone spear: find/witness elite cannibal spear without loot flood.

Each scene should ask Josef only feel questions: readable, scary, fair, alive, too passive, too suicidal, too gamey.
