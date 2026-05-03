# Josef bandit playtest debug intake — 2026-05-03

Status: INTAKE / not yet packaged into Plan.md.
Rule: collect Josef's whole playtest batch first, then ask follow-up classification questions, then package concrete tasks into Plan.md + aux docs without interrupting the active lane.

## Report A — casing pickup drive-by

Josef observed roughly one in-game day earlier: a bandit came by, picked up all ammo casings, and left. No shakedown/demand/combat surface.

Schani live-log read, without touching running game:
- running handoff PID remained active: `17072`, command `cataclysm-tiles --userdir .userdata/dev-harness/ --world McWilliams`.
- recent logs show an active camp-owned outing from `overmap_special:bandit_camp@140,51,0#dispatch` toward `player@140,41,0` / earlier `live_smoke@140,41,0`.
- repeated local gate state: `active_job=scout`, `profile=camp_style`, `posture=hold_off`, `strength=1`, `pack_size=1`, `threat=3`, `opportunity=2`, `margin=0`, `basecamp_or_camp=yes`, `shakedown=no`, `combat_forward=no`.
- source skim: generic `npc::find_item()` + `npc::pick_up_item()` can opportunistically pick visible desirable map/vehicle items within range; this appears separate from bandit shakedown/harvest resolution.

Working classification:
- revised by Josef: picking up stuff is acceptable in principle and fits the collect-bounty system. The bug is not "bandits loot"; the bug is that a lone bandit can walk past / into a camp with three hostile NPCs, collect trivial loot, and leave as if the defended tile were safe.
- design expectation from Josef: collect-bounty behavior must consult immediate local threat/basecamp occupancy/exposure. If the bandit sees or is seen by the player/NPC defenders, especially after Josef steps outside to look at them, it should decide the tile is too hot and leave/reposition rather than continue casual pickup.
- likely implementation seam: generic/local pickup or harvest behavior needs a bandit-specific danger gate before/during pickup, not a blanket ban on pickup.

## Report B — bandit standing too close / idle

Josef observed current live state: one bandit standing in front of the house and not moving.

Schani live-log read, without touching running game:
- repeated local gate state continues to report the active group as `active_job=scout`, `posture=hold_off`, `shakedown=no`, `combat_forward=no`.
- standoff distance oscillates/logs as low as `standoff_distance=2`; Josef says visible position is in front of the house and too close for stalking/holding.
- local gate sees `current_exposure=yes` at some ticks, `recent_exposure=yes`, `basecamp_or_camp=yes`, `threat=3`, `pack_size=1`, `margin=0`.

Working classification:
- likely seam bug: hold_off/stalk/probe posture permits a lone scout to remain too close and visibly idle instead of withdrawing, repositioning to a watch/standoff goal, or escalating correctly.
- Josef clarified this with the same design principle: direct exposure / being looked at by the player at a defended camp should make the scout leave the tile or back off, not freeze close to the house.
- Josef added stronger live observation: there is definitely no sight avoidance right now; the player is looking at the bandit. In stalk mode, being sighted should trigger fallback/backoff.
- If shakedown were intended: it is blocked/wrong because `shakedown=no` and 1v3 should not demand; if stalking were intended: too close and stationary is wrong.

## Evidence anchors

- live log: `.userdata/dev-harness/config/debug.log`, mtime `May 3 13:06:41 2026`, size `2044269`.
- decisive repeated line shape: `local_gate site=overmap_special:bandit_camp@140,51,0 active_group=... target=player@140,41,0 active_job=scout profile=camp_style posture=hold_off strength=1 pack_size=1 threat=3 opportunity=2 margin=0 ... basecamp_or_camp=yes ... shakedown=no combat_forward=no`.
- related dispatch line: second site candidate repeatedly plans `job=scout selected_members=1` but route-missing; active first site blocks redispatch because it already has outside group/contact.

## Design clarification C — root cause framing

Josef narrowed the likely root cause: if bandits do not stalk/hold-off a house by standing directly in front of the door, the casing/loot weirdness largely solves itself. The bandit would not be close enough to collect trivial loot in front of the door.

Implementation implication:
- prioritize sight-aware fallback / minimum standoff for stalk/hold-off around defended houses/basecamps;
- pickup/collect-bounty may not need a broad special ban if the movement/posture layer keeps scouts out of stupid doorstep tiles;
- still useful to interrupt pickup once a tile becomes hot, but the main fix should live in posture/pathing/standoff selection rather than item valuation.

## Report D — standoff regression / canon drift

Josef asked why stalking is at 1 OMT distance and noted this was definitely fixed before.

Schani source/doc audit, without touching running game:
- 2026-04-27 doc `doc/bandit-local-sight-avoid-and-scout-return-cadence-packet-v0-2026-04-26.md` says the correction changed hold-off dispatch goal to a minimum five-OMT visible standoff, with McWilliams expected `live_dispatch_goal=140,46,0`.
- Current source contradicts that: `src/bandit_live_world.cpp::ordinary_scout_watch_standoff_omt()` returns `2`, and `minimum_hold_off_standoff_omt()` returns that value.
- Current deterministic test now asserts `ordinary_scout_watch_standoff_omt() == 2`, `minimum_hold_off_standoff_omt() == 2`, and McWilliams goal `140,43,0`.
- `git blame` shows commit `43c0c5013c feat: tune bandit scout ecology slice` changed the fixed five-OMT minimum back to ordinary scout two-OMT standoff. Earlier commit `a8b2fa0a23d` had the five-OMT correction.
- Live log confirms current behavior crowding to `standoff_distance=2`, with occasional `current_exposure=yes` and no immediate fallback.
- Product matrix later claimed sight-avoid green via `bandit.scout_stalker_sight_avoid_live` with `distance=1`; this was only a guardrail proof and did not preserve the five-OMT product expectation.

Working classification:
- This is likely canon/code drift/regression, not Josef misremembering. The five-OMT doorstep fix existed, then the later scout-ecology tuning intentionally or accidentally loosened ordinary scout/hold-off back to 2 OMT and updated tests to bless it.
- The current failure combines two problems: too-small standoff constant for defended base observation, plus sight-avoid fallback not forcing backoff when the player/base sees the scout.

## Report E — why implemented sight-avoid did not fire in live doorstep case

Josef asked why the sight-avoidance did not fire even though the player was looking at the bandit.

Schani source/log audit, without touching running game:
- Source `src/do_turn.cpp::note_live_bandit_local_turn_sight_avoid()` only considers active members whose saved member state is `member_state::local_contact`.
- It calls `live_bandit_try_sight_avoid_reposition()` only for those `local_contact` members and only for `stalk` / `hold_off` decisions.
- Current live log repeatedly reports `current_exposure=yes` / `sight_exposure=current`, but also `local_contact=no` while the active job remains `scout` / `posture=hold_off`. No `bandit_live_world sight_avoid:` lines appear in the live log during that exposed hover window.
- Therefore the currently exposed scout is being seen by the local gate, but he is not in the state bucket that runs the local-turn sight-avoid reposition helper. The hold_off report is diagnostic/posture selection, not enough by itself to force the outbound NPC to change overmap/local goal.
- The earlier green proof (`bandit.scout_stalker_sight_avoid_live`) staged/repaired an exposed scout/local-contact footing and proved bounded adjacent reposition (`distance=1`) in that seam. It did not prove ordinary outbound scout/hold-off members at 2 OMT or doorstep-scale local positions fall back when sighted.

Working classification:
- Implemented seam was too narrow: sight-avoid exists, but it is wired primarily to saved `local_contact`; normal outbound scout/hold-off exposure can log current sight but skip the actual reposition/fallback.
- Also, even when it fires, current helper chooses one adjacent local step (`distance=1`), not a real retreat to the intended safe standoff. Product expectation now needs: sighted stalk/hold-off member marks tile hot, stops local looting/idle, and re-routes/backoffs toward a safe multi-OMT standoff or home.

## Report F — patrol assignment report spills into in-game log while waiting

Josef debug note: patrol log is still spilling into the in-game log when waiting.

Schani log/source check, without touching running game:
- Current save character log `.userdata/dev-harness/save/McWilliams/#Wm9yYWlkYSBWaWNr.log` does not show `camp patrol` entries; it appears to be the historical character/world log, not the transient message log Josef sees during wait.
- Current debug log `.userdata/dev-harness/config/debug.log` confirms patrol runtime activity during the wait window:
  - `13:19:31.618 INFO : camp patrol: runtime worker=Katharina Leach behavior=loop pos=(3365,992,0) target=(3370,1002,0) route=[(3365,989,0), (3370,1002,0)]`
  - `13:20:27.714 INFO : camp patrol: runtime worker=Katharina Leach behavior=loop pos=(3370,1002,0) target=(3365,989,0) route=[(3365,989,0), (3370,1002,0)]`
- Source `src/npcmove.cpp` around the patrol runtime logs also calls `add_msg( m_info, "[camp][patrol] %1$s patrol assignment: %2$s route to %3$s." )` when `should_show_camp_job_report(... patrol_exception, patrol_trace)` allows it.
- `should_show_camp_job_report()` debounces by worker + report kind + exact signature for 30 minutes. Because `patrol_trace` changes with target/route direction, loop patrols can still emit visible `[camp][patrol] ... patrol assignment` messages during long waits.

Working classification:
- This is likely not just debug-file noise: the source still has a visible `add_msg` for routine patrol assignment changes, so Josef seeing patrol reports in the in-game message log while waiting is plausible/current behavior.
- Desired fix shape: routine patrol route/assignment changes should stay in `DebugLog` / artifacts only, or be hidden behind an explicit debug/verbose camp-report setting. Visible `add_msg` should be reserved for actionable patrol exceptions, not normal loop route changes.

## Report G — spawned northern zombie mass, no horde symbol, roof lamp not attracting

Josef debug note: spawned a horde about 10 OMT north several times; many zombies are visible up north, no horde symbol on the overmap, and a lit lamp on the building roof does not seem to attract them.

Schani source/save/log check, without touching running game:
- Current save overmap `.userdata/dev-harness/save/McWilliams/overmaps/o.0.0.zzip` contains many saved `horde_map` entities. Around player OMT `(140,41,0)`, north-side buckets include:
  - `(137,30,0)` rel `(-3,-11)` count `7`, tracking all `0`
  - `(137,31,0)` rel `(-3,-10)` count `7`, tracking all `0`
  - `(138,31,0)` rel `(-2,-10)` count `3`, tracking all `0`
  - `(140,31,0)` rel `(0,-10)` count `4`, tracking all `0`
- The overmap UI only draws horde glyphs from `overmap_buffer.get_horde_size( ..., horde_map_flavors::active )`; idle/non-active horde entities are ignored for the normal glyph. It also requires blinking/show-hordes/can-see and at least `HORDE_VISIBILITY_SIZE == 3` active entities on that OMT.
- Debug menu `SPAWN_HORDE` uses `overmap_buffer.spawn_mongroup(... GROUP_DEBUG_EXACTLY_ONE, 1)`. That debug group starts as a `mongroup` with default `horde=false` unless manually edited through the overmap horde editor; separate generated `horde_map` entities may still exist/be visible as zombies after local conversion.
- Current recent debug log repeatedly says `bandit_live_world signal scan: signal_packet=no kind=smoke/fire/light ... light_time=daylight`, so the live source scanner is not seeing a viable source packet during the wait window.
- The live C-AOL signal hook observes only map fields `fd_fire` and `fd_smoke` within 60 map squares of the player. It does not inspect ordinary lit item/light-emission state like a lamp. The horde light bridge also returns zero horde signal power in daylight, and requires exposed light with projected range at least 8 OMT.

Working classification:
- No horde glyph is explainable/current behavior if the nearby north zombies are idle horde-map entities or debug-spawned non-horde mongroups rather than active horde-map entries.
- A lit lamp not attracting zombies is also explainable/current behavior: implemented roof-horde proof covered player-created roof fire / `fd_fire` signal, not arbitrary lit lamps. If lamps are supposed to work as zombie bait, that is a new product gap: local light-emission/item-light state needs an adapter into the overmap/horde signal path, plus night/day and roof exposure rules.
- If the intended debug recipe is “spawn a visible active horde 10 OMT north and watch it walk to a roof light,” the current debug spawn/editor path is misleading and needs either a better active-horde fixture/tool or clearer debug UI wording.

Follow-up after Josef: Josef is right to flag this as something Andi should probably have wired, at least in spirit. Older concept canon explicitly talked about ordinary lamps/lit windows as meaningful visible-light sources, while the closed implementation/proof narrowed to local `fd_fire` / `fd_smoke` fields and roof/elevated brazier fire. Classify the lamp case as an implementation coverage gap / over-narrow adapter, not a brand-new unrelated feature request.

## Report H — night/fog live light visibility status check

Josef asked for current live status after saying it is now night and foggy, and asked who can/cannot see the roof lamp/light.

Schani log/save check, read-only:
- Recent debug log at save/log mtime `2026-05-03 13:38` repeatedly reports `bandit_live_world signal scan: signal_packet=no kind=smoke/fire/light scan_radius_ms=60 ... light_time=night light_weather=clear` during the night window.
- No recent `bandit_live_world horde light signal:` lines appear after the lamp setup.
- No recent `zombie_rider camp_light:` lines appear after the lamp setup.
- Dispatch lines still come from `candidate_reason=direct_player_range` and `signal_packet=none`, not from a visible light/signal packet.
- Current recognized hostile sites in the live dispatch logs are `overmap_special:bandit_camp@140,51,0` and `overmap_special:bandit_camp@140,51,1` at distance `10` from `player@140,41,0`.
  - `@140,51,0` is repeatedly rejected because it already has an active outside group/contact.
  - `@140,51,1` repeatedly plans a scout from direct player range, then rejects with `reason=route_missing`.
- The weather adapter as logged says `light_weather=clear`, despite Josef describing visible fog. This may be a perception/weather-band mismatch worth checking separately; it does not change the immediate lamp result because no light packet is being emitted at all.

Current readout:
- Nobody is seeing the lamp through the C-AOL live signal system right now.
- Bandit sites are reacting/attempting to react to direct player proximity, not to the lamp.
- Zombies/hordes are not receiving a horde-light signal from the lamp.
- The lamp case remains the same adapter gap as Report G: the live scanner sees `fd_fire`/`fd_smoke`, not lit lamp/item/furniture light emission.

## Report I — returned scout loop sends another single scout instead of escalating to big-camp shakedown

Josef debug note: A bandit stood near the camp/base, left, then the camp sent back a single scout. For a big camp with enough people, Josef expected a larger dispatch and a shakedown / shake-us-down response, not another lone scout.

Schani source/log check:
- Live logs show the first active outing as `active_job=scout`, `strength=1`, `pack_size=1`, `posture=hold_off`, `shakedown=no`, with `threat=3`, `opportunity=2`, `margin=0`, `basecamp_or_camp=yes`, and recent/current sight exposure at times.
- When the scout returns home, logs show `scout_report: returned -> pressure refreshed ... remaining_pressure=ample`.
- Immediately afterward, the same site plans again as `job=scout selected_members=1 signal_packet=none candidate_reason=direct_player_range`.
- The local gate never opens shakedown in this state because direct-player scout pressure has only one member and margin stays too low; basecamp/recent exposure also pushes the local gate into `hold_off` before the open-shakedown branch.
- Source cause: `make_nearby_target_lead()` makes ordinary bandit camp direct-player pressure a `lead_family::site`, hard-blocks `scavenge`, `steal`, and `raid`, and `lead_family::site` does not include `toll`; this leaves only `scout` as the normal winning handoff for direct player range. `required_dispatch_members_for_profile()` only scales cannibal attack pressure; ordinary `camp_style` uses generic required members, so a big camp does not automatically upgrade direct-player pressure into a 2+ member toll/shakedown party.

Working classification:
- Josef expectation is right for product feel: a large camp that already probed the base and still has ample pressure should be able to escalate from scout/hold-off into a larger toll/shakedown dispatch.
- Current implementation is structurally biased into a lone-scout loop: scout returns -> pressure refreshed -> direct-player lead remains site/scout-only -> one more scout.
- This should become a follow-up item for camp-style escalation policy, not merely tuning one number.

Possible desired fix shape:
- Add a post-scout / scout-confirmed camp-style escalation path: if camp pressure remains `ample`, roster/dispatchable manpower is enough, and the target/basecamp remains valuable/reachable, promote the next dispatch to `toll` or an explicit `shakedown`/pressure party.
- Scale ordinary camp-style dispatch size for shakedown/toll from available roster while preserving a home reserve.
- Ensure local gate can open shakedown for the larger party when contact is established, rather than repeatedly holding off with `strength=1 margin=0`.
- Keep small/weak camps and high-threat scenes cautious, so every camp does not suicide-rush the base.

## Report candidate — shakedown still has three answer options instead of two

Josef debug note: shakedown still presents three answer options, not the expected two. Existing closed scenic shakedown canon/proof currently claims `Pay / Fight / Refuse`; Josef's live read says that three-option surface is wrong. Treat as a UI contract correction/regression: the shakedown choice should expose two actionable answers, assumed pay and fight unless Josef clarifies otherwise. Refuse/cancel/back-out behavior, if needed internally, should not appear as a third answer in the visible shakedown choice surface.

Validation shape:
- deterministic/dialogue-surface test: generated shakedown responses are exactly two visible choices for first and reopened shakedowns;
- live proof: normal dialogue window screenshot/OCR shows only the two expected choices;
- regression: pay path still works, fight path still works, no-shakedown/cannibal regression remains green.

## Report candidate — shakedown pay does not open trade window with debt

Josef debug note: choosing “Pay the demanded goods” does not open a trade window with debt. Current source confirms the bug/contract mismatch: the scenic shakedown dialogue returns `pay`, then `open_live_bandit_shakedown_surface()` calls `live_bandit_surrender_goods()` directly. That path auto-erases reachable goods by value from player/nearby allies/ground/vehicle and never opens `npc_trading::trade`, never calls `pay_npc`, and never represents the demanded toll as trade debt/cost.

Expected shape:
- visible pay choice opens the normal NPC trade window or a trade-equivalent selector;
- demanded toll appears as debt/cost so the player can choose which goods to surrender;
- successful chosen payment writes the same paid/writeback/return-home outcome;
- insufficient/cancelled payment falls into the intended fight/refusal path without hidden confiscation;
- pair this with the separate two-visible-options correction: shakedown surface should show the intended two choices, and the pay choice should bridge into trade/debt UI.

Validation shape:
- deterministic: pay branch opens/requests trade with demanded value as debt/cost instead of calling auto-surrender;
- live: selecting pay shows the trade/debt UI with demanded value visible;
- live: successful chosen payment records `shakedown_surface paid`; cancel/insufficient payment remains combat-real;
- regression: fight path, reopened demand, and no-shakedown/cannibal rows remain green.

## Report candidate — writhing stalker too passive after threat drops

Josef debug note: writhing stalker may now be too passive. As soon as local threat goes down, it should swoop in quite fast. This is not a request to make it suicide-rush through high-threat visibility; it is a timing/threshold correction for the favorable-window transition.

Expected behavior:
- while defender/player/NPC threat is high, stalker holds, withdraws, reroutes, or uses cover/darkness;
- when zombies engage defenders, defenders die, line pressure drops, or the player becomes distracted/vulnerable, the stalker quickly converts from stalk/hold into approach/pounce/short strike burst;
- zombie engagement/threat reduction should become target/opportunity footing, not only a modifier after an unrelated `mon_plan.target` already exists;
- after the distraction ends or threat rises again, it should leave/re-cover instead of remaining exposed.

Validation shape:
- high-threat baseline: no reckless rush;
- threat-drop transition: bounded short-cadence swoop/pounce/strike;
- zombie engagement creates target/opportunity evidence from no-target state;
- one-zombie and multi-zombie thresholds tested separately;
- post-threat-rise retreat/cover/stalk reset.

### Josef clarification — visible shakedown choices

Josef clarified: visible shakedown choices should be exactly Pay and Fight. Refuse is redundant with Fight and should not be a third visible answer. Treat backout/cancel, if supported, as UI behavior rather than a product-level third shakedown response.

## Investigation addendum — prior shakedown notes were stranded and misleading success canon exists

Josef asked whether the shakedown two-option and pay/trade/debt notes had already existed and were supposedly fixed. Result: yes, both existed in the 2026-05-02 Discord playtest intake and were written to workspace-only raw file `/Users/josefhorvath/.openclaw/workspace/runtime/caol-bandit-playtest-intake-2026-05-02.md`:

- note 1: surface showed `refuse demand`, `fight`, and `pay`; Josef read was two choices only;
- note 4: pressing `pay` did not open a trade window; expected trade window with player owing the demanded toll from the start.

They were not promoted into repo canon before the scenic shakedown lane was closed. Current repo canon instead contains the misleading success chain:

- `doc/bandit-shakedown-pay-or-fight-surface-packet-v0-2026-04-22.md` originally described the product as a blunt `pay` vs `fight` fork and a “trading-style surrender surface”, but also scoped out a “fake debt economy”; actual implementation still used auto-surrender.
- `doc/bandit-scenic-shakedown-chat-window-openings-packet-v0-2026-05-01.md` accidentally/incorrectly changed the success language to `pay/fight/refuse`.
- `456ab43d65d` added the scenic `dialogue_window`, visible `Refuse the demand`, and `responses=pay/fight/refuse` log.
- `797725e2413` closed `Plan.md:326` and proof doc with `Pay / Fight / Refuse` as green evidence.
- the pay branch remains the Apr 24 `live_bandit_surrender_goods()` auto-confiscation path; no trade/debt UI was implemented.

Classification: not fixed. The debug notes were effectively stranded in raw workspace intake and then contradicted by later repo success wording/harness artifact expectations. Treat this as a canon/proof hygiene failure plus a real shakedown UI/payment bug.

## External report candidate — debounce NPC sorting retries

Relayed by Josef, not from our playtest: sorting changed and now “sucks big time”; about half the time NPCs cannot do it, and then they retry extremely rapidly / effectively thousands of times per second. Needs at least a debounce.

Quick source seam:
- `npc::find_job_to_perform()` assigns `zone_sort_activity_actor()` whenever `ACT_MOVE_LOOT` has priority;
- `basecamp::start_menial_labor()` also directly assigns `zone_sort_activity_actor()`;
- `zone_sort_activity_actor` has per-activity `unreachable_sources`, but if the activity fails/ends and the NPC job loop creates a fresh sort activity next turn, that per-actor memory/cooldown is lost;
- likely fix belongs at NPC job/basecamp activity assignment and/or persistent sorting failure state, not merely inside one transient sort actor pass.

Expected behavior:
- when an NPC cannot sort because destinations are unreachable/full/invalid, items are too large/heavy, no valid sort work exists, or pathing fails, it records a bounded blocked/debounce state;
- the same NPC should not immediately reassign/retry sorting every turn against the same failing setup;
- retry becomes allowed after a cooldown, relevant state changes, or explicit player/job retry;
- visible/debug log spam should be bounded and useful.

Validation shape:
- deterministic failing NPC sort setup: one attempt records cooldown, repeated job scan does not recreate `ACT_MOVE_LOOT` every turn;
- recovery test: adding reachable destination/freeing inventory/zone change or cooldown expiry allows retry;
- live/log test: waiting near failing NPC sorter produces bounded messages/logs, not per-turn spam;
- successful normal sorting remains green.

### Josef clarification — lamp/household light belongs in the live signal adapter

Josef clarified the lamp note from package 7: lamp lights should also be included here. The live adapter should not be limited to fire / `fd_fire` / fire-derived light. Ordinary visible light should count as light, same as fire, when it is exposed/bright enough.

Actor-specific interpretation:
- **Bandits** should distinguish search lights / directional scanning threat lights from household lights / fires. Search lights lean threat/active defense; household lights/fires lean occupancy/bounty/camp evidence, with weather/containment/range still applying.
- **Zombies/hordes** do **not** make that distinction. For them, visible exposed light is attraction pressure whether it comes from fire, a lamp, household/window light, or comparable bright source.

Classification update for Report G/H: current `fd_fire`/`fd_smoke`-only live scanner is an over-narrow adapter bug/coverage gap. The expected fix should add ordinary lit item/furniture/appliance/local-light emission into the same bounded light-signal bridge, while preserving bandit semantic distinction and zombie/horde non-distinction.

Validation additions:
- roof/exposed lamp at night emits a live light packet without `fd_fire`;
- zombie/horde bridge receives horde-light signal from lamp/household light using the same broad attraction category as fire;
- bandit mark generation classifies household/fire light separately from searchlight-like directional threat light;
- daylight/contained/weak-light suppression remains bounded;
- existing roof `fd_fire` proof remains green.

## Josef classification answers — 2026-05-03 debug batch

Josef answered the follow-up classifications:

1. **Shakedown visible choices:** exactly Pay + Fight. Esc/backout is refuse, and refuse means fight. With bandits, refusing and fighting are the same product branch; do not show `Refuse` as a third visible answer.
2. **Pay flow / reachable goods:** payment should include carried goods, basecamp goods, and basecamp NPC carried items — broadly everything around that is inside the faction / honest basecamp-side reachable pool. Josef noted this was specified before.
3. **Defended-camp standoff:** restore/keep about 5 OMT minimum for normal scout/hold-off near camp/house. Product read: they are watching the house through binoculars; when actual standoff/contact begins, they move in.
4. **Lamp / ordinary light:** all light sources should attract zombies and bandits depending on weather and lighting. Not only actual fires/smoke. Bandits can distinguish search lights from household lights/fires; zombies/hordes do not.
5. **Patrol aggression / camp alarm:** camp patrol NPCs should attack hostile bandits/zombies on sight, not neutral/unknown travelers. Ideally hostile sighting puts the whole camp into alarm state: every NPC independent of shift is put on patrol roster if they have the job listed in their responsibilities.

Packaging implication: treat these as product contract corrections, not optional new feature drift. Several notes were previously specified or implied by older packets but were narrowed/overwritten by later implementation/proof language.

## Follow-up clarification — stalker and stalking-mode LoS/rhythm

Josef clarified after packaging: do not forget that the writhing stalker still needs behavior touch-up beyond generic threat-drop timing. Required shape:
- bigger stalking distance than the prior close v0 footing;
- sight avoidance that actually happens, not visible loitering with a stalking label;
- same sight-avoidance rule for bandits in stalking mode;
- faster conversion into attack/pounce when the window opens;
- after the strike/burst, quickly boot back out into cover/stalking/recovery instead of idling exposed.

Classification: fold into `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, strengthening Slice 2 and Slice 7.

## Follow-up request — debug spawn horde / stalker / rider options

Josef requested more debug spawn options for live testing:
- `spawn horde` should actually create a medium-sized horde;
- add horde distance options including `5` OMT and `10` OMT;
- add writhing stalker options at `5` OMT and `10` OMT;
- add zombie rider options at `5` OMT and `10` OMT.

Classification: fold into `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` as a debug-helper slice at the bottom of the current execution stack, without interrupting the active shakedown-first order.

## Follow-up clarification — transpose stalking/escalation to cannibals and compatible hostiles

Josef clarified that any behavior that can be transposed to other entities should be transposed. In particular:
- cannibals should also stalk at about `5` OMT where terrain/pathing permits;
- cannibal stalking should use real sight avoidance rather than visible loitering;
- after they judge they can overpower the defenders, cannibals should send an attack dispatch, possibly a large/whole-camp commitment depending on roster, odds, distance, and reserve policy;
- preserve profile-specific outcomes: bandits can shakedown/toll; cannibals attack rather than negotiate.

Classification: fold into `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0`, strengthening Slice 2 and Slice 4 and adding a general transposition rule for compatible hostile profiles.

## Follow-up request — cannibal Monsterbone spear lore item

Josef requested a cannibal item: `Monsterbone spear`. Desired read:
- fairly strong spear, sturdy because it is made from huge monster bone, but not overpowered;
- lore prop that implies cannibals ate monster meat and went mad/changed;
- the monsters are huge and dungeon-like, with brain interiors, so the bone should feel impossible and memorable;
- place maybe `1` or `2` in cannibal camps;
- let a few cannibals wield it, not every cannibal.

Classification: fold into `CAOL-JOSEF-LIVE-DEBUG-BATCH-v0` as a bottom equipment/lore slice after the live-behavior/debug-helper work.

### Monsterbone spear refinement — ritual / elite distribution

Josef clarified the Monsterbone spear should be ritualistic/status-coded, not an everyone weapon:
- elite spear, pretty good and sturdy, but not OP;
- `1` or `2` in a cannibal camp;
- maybe wielded by a couple important people from the camp;
- not common cannibal gear.

## Follow-up request — full deterministic/playtest scenario matrix

Josef asked to think about testing for all package points: deterministic internal switch tests, staged/live playtests, and edge cases across bandits, writhing stalkers, cannibals, zombie riders, hordes/lights, and vertical player positions such as first floor/roof. Specific emphasized questions:
- does a stalker swoop in when threat drops;
- what does a stalker do when the player hunts it;
- what happens when the player is on the first floor/upstairs/roof;
- which behavior can be covered by deterministic internal switch tests versus playtesting.

Classification: added `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md` as the package validation matrix.

## Follow-up request — writhing stalker final flavor sentence

Josef selected this exact final sentence for the writhing stalker description:

> It’s the eyes that do it for you, not the dripping teeth.

Implementation note: actual JSON may use ASCII apostrophe if needed, but the sentence must keep `do it for you` and `not the dripping teeth`.

## Follow-up request — smoke-out and high-z tower edge cases

Josef asked to include player-vs-hostile edge cases in the active test matrix:
- what if the player smokes out a suspected bandit stalking location;
- same for cannibals;
- what if the camp/player is in a tower around `z=5`;
- keep thinking in deterministic switch tests first, with live/playtest rows for product behavior/feel.

Classification: folded into `doc/josef-live-debug-batch-test-matrix-v0-2026-05-03.md`, `TESTING.md`, `TODO.md`, `SUCCESS.md`, and handoff docs.
