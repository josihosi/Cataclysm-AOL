# Bandit camp-map ecology code audit (2026-04-28)

Status: SCHANI AUDIT OF ANDI IMPLEMENTATION

Audited against:

- `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`
- `doc/bandit-camp-map-ecology-implementation-map-2026-04-28.md`
- implementation commit: `224fa86fa9 Add camp intelligence map scout writeback`

Audit question used for every item:

> Will this make the player experience in Cataclysm-AOL match the source of truth: a hungry camp with its own dirty saved map that notices bounty before threat, sends eyes, learns, eats finite bounty, refreshes living-human opportunity, remembers danger, expands when hungry, and carries consequences across overmap/local seams?

Validation actually run / observed:

- `./tests/cata_test "[bandit][live_world][camp_map]"` passed: 43 assertions in 2 test cases.
- `./tests/cata_test "[bandit][live_world]"` passed: 628 assertions in 27 test cases.
- No live GUI/player smoke was run for this audit.

## High-level verdict

Andi landed a real foundation slice, not decorative paperwork. The code now has a saved per-camp `intelligence_map`, persistent `camp_map_lead` records, enum conversion, stable-ish lead ids, upsert, scalar-memory migration, active target OMT persistence, and scout-return writeback into the map.

But the source-of-truth creature is not alive yet. Current gameplay is still mostly: live signals and direct player range feed the old scalar dispatch path, a scout can go out, local gate logic can stalk/hold/probe/shakedown/attack/abort, and a surviving scout return writes one camp-map lead. That is useful scaffolding. It is not yet: map-driven cadence, bounty-before-threat ecology, finite structural harvest, route memory, frontier exploration, or full lead-bound overmap/local consequence flow.

Short version: **green for saved camp-map foundation; yellow/red for player-facing ecology.**

## Item-by-item implementation audit

### 1. Saved per-camp map structs and enum conversion

Verdict: PARTIAL / GOOD FOUNDATION.

Implemented:

- `site_record` owns `camp_intelligence_map intelligence_map`.
- `camp_intelligence_map` persists schema/tick/radius fields plus `std::vector<camp_map_lead> leads`.
- `camp_lead_kind` and `camp_lead_status` exist with string conversion.

Missing:

- No `camp_map_cell`.
- No `camp_route_memory`.
- `camp_map_lead` lacks planned structural/human lifecycle fields like `harvested_value`, `remaining_structural_bounty`, and `refresh_pressure`.

Player-experience answer: this starts making each camp capable of owning its own dirty map. It does not yet let the camp remember eaten ground, checked-empty places, no-path cells, or human routes.

Needed test / inspection:

- Once added: save/load proof for cells and routes, not just leads.

### 2. Serialization / deserialization and old remembered-field migration

Verdict: PARTIAL.

Implemented:

- `camp_map_lead` serializes/deserializes.
- `camp_intelligence_map` serializes/deserializes.
- `site_record` writes/reads `intelligence_map`.
- Tests prove two different camps keep distinct lead memories across save/load.
- Tests prove old scalar memory can migrate into one fallback lead.

Risk:

- Migration currently runs when `intelligence_map.leads` is empty. The implementation doc says migrate when no `intelligence_map` exists. A modern save with an explicit empty map plus old scalar compatibility fields may sprout a ghost legacy lead on load. Sehr elegant, the old scalar pantry mouse survives the exterminator.

Player-experience answer: persistence is real, but migration can over-create memory the new map did not earn.

Needed test:

- JSON missing `intelligence_map` plus old `remembered_*` fields => migrate.
- JSON with explicit empty `intelligence_map: { "leads": [] }` plus old `remembered_*` fields => should not migrate unless this is deliberately chosen.

### 3. Stable ids and create/update/dedupe helpers

Verdict: PARTIAL.

Implemented:

- `camp_lead_id_for()` derives from site id, kind, target id, and OMT.
- `upsert_camp_map_lead()` dedupes by `lead_id`.

Missing:

- No public/tested helper proves repeated generated lead ids remain stable across save/load.
- Tests mostly hand-author lead ids except the scout-return path.
- No active outing carries `lead_id`, so stable lead identity is not yet the spine of dispatch/handoff.

Player-experience answer: the data can avoid duplicate notes, but the camp does not yet act as if every outing belongs to a particular map note.

Needed tests:

- Same camp + same target + repeated scout return after save/load updates one lead, not two.
- Two camps observing the same target produce separate camp-owned lead ids.

### 4. Sparse structural bounty / cell estimation

Verdict: NO.

Implemented:

- Nothing beyond enum names for structural-ish lead kinds.

Missing:

- No `camp_map_cell`.
- No structural estimate by overmap terrain/site class.
- No finite `remaining_structural_bounty`.
- No harvested/checked-empty/no-path dampening.

Player-experience answer: no. The world is not yet being eaten in camp memory. A looted farmhouse cannot become quieter forever because that memory bucket does not exist.

Needed tests:

- Abstract offscreen cabin/farm/store harvest reduces finite camp-side bounty and suppresses immediate repeat exploitation.
- Loaded local loot path does not double-count with abstract harvest.

### 5. Signal adapter writers for smoke/light/human/activity leads

Verdict: PARTIAL, but not map-backed.

Implemented:

- Live fire/smoke/light scanning near the player exists in `do_turn.cpp`.
- Smoke/light observations use mark-generation adapters with weather/time/exposure shaping.
- Matching sites record bounded `live_signal_mark`s via `record_live_signal_mark()`.
- Hordes can be signaled from light observations.

Missing:

- `record_live_signal_mark()` writes old scalar fields: `remembered_target_or_mark`, bounty/threat estimates, and `known_recent_marks`.
- It does not create `smoke_signal` / `light_signal` camp-map leads.
- No human/basecamp route/activity writers into map leads.
- No per-camp lead source shape beyond the scalar mark string.

Player-experience answer: player fire/smoke/light can attract/refresh bandit pressure, but not yet as dirty per-camp map notes. The camp hears a bell; it does not yet draw the charcoal mark in the new map.

Needed tests:

- Player-lit smoke/fire creates a `smoke_signal` or `light_signal` lead on each eligible camp map with source OMT, bounty, confidence, weather-shaped range, and no exact inventory/player truth.
- Repeated human/basecamp routine refreshes a human/basecamp lead without smearing into structural terrain bounty.

### 6. Distance-band cadence scheduling

Verdict: NO.

Implemented:

- Fields exist: `next_near_tick_minutes`, `next_mid_tick_minutes`, `next_far_tick_minutes`, `next_frontier_tick_minutes`.

Missing:

- No camp-owned perception profile.
- No near/mid/far/frontier wake loop.
- No due-band evaluation.
- Dispatch is still triggered from direct player range / live signal candidates, not camp cadence.

Player-experience answer: no. Camps still behave more like event/range responders than attention-limited animals with distance-shaped awareness.

Needed tests:

- A near lead updates/dispatches earlier than a far lead.
- Far signals remain remembered but are reconsidered slowly.
- No every-turn omniscient scan of every mark.

### 7. Lead aging, soft decay, checked-empty, harvested, confirmed-threat rules

Verdict: NO / VERY PARTIAL.

Implemented:

- Lead fields can store times, status, confidence, `threat_confirmed`, `times_checked_empty`, `times_harvested`.
- Scout return can set a confirmed-ish basecamp lead.

Missing:

- No aging/decay routine.
- No false lead or checked-empty dampening.
- No harvested lifecycle.
- No confirmed-threat downgrade only by recheck.

Player-experience answer: no. Threat can be stored on a lead, but scar-tissue behavior is not implemented.

Needed tests:

- Soft smoke signal weakens/disappears after vanishing.
- Scout-confirmed threat persists over time.
- Threat downgrades only after safe recheck/explicit return packet.

### 8. Frontier exploration when hungry and ignorant

Verdict: NO.

Implemented:

- `frontier_probe` enum value and `next_frontier_tick_minutes` field only.

Missing:

- No hunger/low-bounty driver.
- No frontier ring/arc selection.
- No explore outing.
- No checked-empty/structural/no-path result from exploration.

Player-experience answer: no. The hungry camp does not yet widen its known world when the cupboard is empty.

Needed tests:

- Empty map + low stock/bounty pressure => bounded scout/explore, not random wandering.
- Empty map + stable stock => hold/chill.

### 9. Roster/reserve/stockpile ledger inputs

Verdict: PARTIAL.

Implemented:

- Dispatch uses live roster counts.
- Home reserve exists per profile.
- Outbound/local members block dispatch.
- Lazy materialization creates concrete members from abstract headcount for eligible sites.

Missing:

- No stockpile/bounty pressure ledger.
- No recent gains/losses as first-class dispatch inputs, except some shakedown aftermath state.
- No food/ammo/med placeholder pressure fields.

Player-experience answer: manpower logistics are real enough to prevent camps emptying themselves like idiots. Hunger/stockpile ecology is not real yet.

Needed tests:

- Camp with only reserve cannot dispatch.
- Wounded/unready/outside members reduce dispatch capacity.
- Future: low stockpile changes intent toward exploration/risk, high stockpile dampens pressure.

### 10. Intent before count / dispatch from map leads

Verdict: NO / OLD PATH STILL DOMINATES.

Implemented:

- `plan_site_dispatch()` evaluates a dry-run lead and then sizes/selects members.
- Current bounded v0 only promotes scout-style pursuit from real owned members.

Missing:

- It receives `target_omt` and `target_id` directly.
- It builds a synthetic nearby target lead via `make_nearby_target_lead()`.
- It does not select intent from `site.intelligence_map.leads`.
- It does not choose among hold/explore/scout/rescout/harvest/stalk/toll/steal/raid based on lead status/kind.

Player-experience answer: partial pressure exists, but not the source-of-truth loop. The camp is still handed a target; it is not reading its own dirty map and choosing what that note means.

Needed tests:

- Given multiple saved leads, dispatch chooses intent/target from the best map lead, not from caller-provided player position alone.
- Stale soft lead => re-scout/explore, not raid.
- High confirmed threat suppresses raid unless desperation explicitly wins.

### 11. Durable active group state tied to `lead_id`

Verdict: PARTIAL / MOSTLY NO FOR LEAD LINKAGE.

Implemented:

- Existing active fields persist: `active_group_id`, `active_target_id`, `active_target_omt`, `active_job_type`, `active_member_ids`, sortie timestamps.
- Tests cover active sortie timestamp save/load.

Missing:

- No durable active group struct.
- No `lead_id` on active outing.
- No saved route/posture/cargo/burden/wounds/panic/local join key beyond older fields.

Player-experience answer: save/load can preserve that someone is out. It cannot yet preserve “this group belongs to this exact camp-map lead and is bringing this exact consequence back.”

Needed tests:

- Save/load while group is outbound/local/returning preserves associated `lead_id` and return budget.
- Future cargo/wounds/panic survive save/reload.

### 12. Scout return writes camp-map leads

Verdict: YES FOR ONE NARROW PATH / PARTIAL OVERALL.

Implemented:

- `record_scout_return_lead()` writes a `basecamp_activity` lead on surviving scout return.
- It records target id, target OMT, group source, started/local-contact times, bounty/threat/confidence, target-alert/scout-seen, losses, and last outcome.
- Test proves scout return creates a persistent camp-map lead.

Missing:

- Only scout returns with survivors write a map lead.
- Non-scout returns, scout death, no-path, empty, harvest, threat-only, route, and cargo outcomes do not write the map properly.
- Bounty/threat values still come from scalar remembered fields.

Player-experience answer: this is the first real bite of the source-of-truth loop: eyes can make memory. But it is one little mouthful, not the full animal.

Needed tests:

- Scout sees basecamp and returns => lead persists after save/load.
- Scout dies/missing => threat/loss lead is written without omniscient full target truth.
- Scout returns empty/no-path => false/empty/no-path dampening is written.

### 13. Ordinary harvest/scavenge return reduces finite structural bounty

Verdict: NO.

Implemented:

- No structural bounty bucket to reduce.

Player-experience answer: no. Bandits cannot yet eat finite ground loot in the camp map.

Needed tests:

- Scavenge return marks structural lead/cell harvested and lowers repeat appetite.
- Local item removal and abstract bucket resolution do not double-pay.

### 14. Human/basecamp activity refreshes mobile/site bounty

Verdict: PARTIAL VIA OLD SIGNAL PRESSURE, NO VIA MAP ECOLOGY.

Implemented:

- Player-near smoke/light activity can refresh scalar remembered marks and provoke dispatch candidate selection.
- Scout return writes `basecamp_activity` lead.

Missing:

- No recurring human/basecamp activity lead refresh based on routines, routes, work, storage, vehicle/cargo movement, NPC movement.
- No source identity tracking beyond broad player target id / mark id.

Player-experience answer: the player can attract pressure with visible activity, but the camp does not yet learn a living-site routine that keeps refreshing opportunity.

Needed tests:

- Repeated basecamp work/light/smoke refreshes the same site lead and increases confidence without fabricating exact inventory.
- Player/basecamp moving creates new route/activity lead while old lead ages rather than immortal-pursuing.

### 15. Threat recheck/update only from close observation/local outcome

Verdict: PARTIAL FIELD, NOT POLICY.

Implemented:

- Lead stores `threat` and `threat_confirmed`.
- Scout return can mark threat confirmed from scalar estimate or still-valid resolution.
- Local gate uses local threat/opportunity to decide posture.
- Losses from return packet can be counted for scout lead prior bandit losses.

Missing:

- No general policy that confirmed threat never passively decays.
- No downgrade only by close contradictory observation/safe passage.
- No zombie/trap/fortification/defender evidence path into threat leads.

Player-experience answer: local caution exists, but durable scar-tissue threat is not implemented yet.

Needed tests:

- Bandit loss raises threat/caution on associated lead.
- Confirmed threat persists over days.
- Safe recheck can downgrade; timer alone cannot.

### 16. Overmap-to-bubble entry payload from active group + lead

Verdict: NO / VERY PARTIAL.

Implemented:

- Existing handoff entry payload works from `abstract_group_state`.
- Dispatch group includes source camp, group strength, target, scalar threat/bounty estimates, retreat/cargo/return values, anchored identities.

Missing:

- No `lead_id` in payload.
- No camp-map lookup when building payload.
- Local side receives scalar target memory, not the full “what this camp believes about this map lead.”

Player-experience answer: local bandits can enter the world with bounded mission pressure, but not yet as emissaries of a specific dirty map note.

Needed tests:

- A chosen camp-map lead produces entry payload fields: lead id, intent, bounty/threat/confidence, scout_seen/target_alert, retreat bias, return clock.
- Local side does not know exact truth beyond lead belief.

### 17. Local scout/stalker posture, sight avoidance, target-alert writeback

Verdict: PARTIAL.

Implemented:

- Local gate posture chooses stalk / hold_off / probe / open_shakedown / attack_now / abort.
- Hold-off standoff goal can use two OMT.
- Sight-avoid reposition helper exists and is called for stalk/hold-off local-contact members.
- Scout sortie return timer exists; tests cover around 180-minute limit.

Missing:

- The source-of-truth doc asks for roughly half-day watch for current lane; current tested scout return helper uses 180 minutes in unit tests and live `scout_sortie_limit_minutes` needs to stay honest against the lane contract.
- No explicit “by two visible local turns moved/aborted/reported blocked” proof tied back to map lead.
- No `scout_seen`/`target_alert` writeback except narrow scout-return resolution/posture mapping.

Player-experience answer: local behavior has the right flavor of cautious contact, but the visible-scout consequences are not yet reliably written back to the map lead.

Needed tests:

- Live scout visible for repeated turns attempts reposition or aborts/returns without teleporting.
- Exposure writes `scout_seen`/`target_alert` to the associated lead.
- Lane clarifies and tests 180 minutes vs half-day watch requirement.

### 18. Bubble-to-overmap return packet into map, ledger, group/member state

Verdict: PARTIAL.

Implemented:

- `apply_return_packet()` validates source/group, updates member state, clears active group, applies scalar group memory, and calls `record_scout_return_lead()`.
- Tests cover return packet flows and camp-map scout lead creation.

Missing:

- Generic return packet writeback into existing lead/cell/route.
- Cargo to stockpile/need ledger.
- Structural harvest reduction.
- Wounds/panic/cargo persistence in active group.
- Threat recheck/update from local outcome across all outcome types.

Player-experience answer: headcount and one scout-memory consequence survive. The rich “they looted, got hurt, got scared, learned, and brought it home” ecology does not yet.

Needed tests:

- Return packet with cargo reduces structural bounty or records camp stockpile gain.
- Return packet with losses strengthens associated lead threat.
- Return packet with target lost/empty/no path updates lead status and suppresses immediate repeat stupidity.

### 19. Cleanup/caps

Verdict: NO FOR MAP, PARTIAL FOR OLD MARKS.

Implemented:

- `last_daily_cleanup_minutes` field exists.
- `known_recent_marks` has a cap of 8.

Missing:

- No daily camp-map cleanup.
- No cap on `intelligence_map.leads`.
- No duplicate collapse beyond exact-id upsert.
- No stale low-value signal pruning.

Player-experience answer: harmless while writers are few, but once real signal writers exist long worlds can collect map sludge.

Needed tests:

- Stale weak signals are pruned/capped.
- Confirmed threat, harvested/checked/no-path dampening, and recurring human activity survive cleanup.

### 20. Readable reports / observability

Verdict: PARTIAL.

Implemented:

- Existing reports/logs cover local gate posture, signal scan/maintenance, dispatch rejections, dispatch goal, scout return, shakedown surface, and empty-site retirement.
- Tests inspect some report strings.

Missing:

- No report renders camp-map leads, lead id, chosen map intent, cadence band, cleanup, or map writeback outcome.
- Debug still mostly speaks scalar dispatch language.

Player-experience answer: developer observability is decent for the old bounded scout path; not yet good enough to debug the new dirty-map ecology without spelunking save JSON.

Needed tests / probes:

- Debug/report helper lists per-camp leads with kind/status/bounty/threat/confidence/source/age.
- Dispatch report names chosen `lead_id` and intent.
- Return report names lead writeback and outcome.

## Edge-case audit

### Nothing visible / no useful leads

Verdict: NO.

Current behavior: dispatch may skip if no live signal/direct range candidate. No hold/chill vs hungry-frontier decision.

Needed test: empty map + low stock => frontier probe; empty map + stable stock => hold.

### Signal vanishes

Verdict: NO / PARTIAL OLD MARK MEMORY.

Current behavior: old scalar marks remain; no map soft-confidence weakening.

Needed test: smoke/light vanishes; soft signal lead weakens, but scout-confirmed human/basecamp lead remains actionable.

### False lead / empty check

Verdict: NO.

Needed test: scout checks empty source; writes false/checked-empty dampening and suppresses immediate re-dispatch.

### No path

Verdict: NO.

Current behavior: live dispatch rejects route_missing, but no lead/cell no-path mark is written.

Needed test: route failure writes no-path/route-blocked timestamp, not random wandering.

### Camp has no home members

Verdict: YES/PARTIAL.

Implemented: home reserve and empty-site retirement logic respect active outside pressure. Tests exist around retirement.

Needed test: active outside/local/returning members prevent retirement across save/load.

### Camp has active outside group

Verdict: YES FOR FAIL-CLOSED V1.

Implemented: outbound/local members block more dispatch from same camp. This is overbroad but acceptable for v1.

Needed test: active group remains blocked after save/load; unrelated dispatch remains blocked unless explicitly supported later.

### Scout dies

Verdict: PARTIAL FOR MEMBER STATE, NO FOR MAP THREAT.

Implemented: dead/missing member state can be recorded and return packet can count losses.

Missing: death does not reliably write a threat/loss lead unless a survivor scout-return lead exists.

Needed test: scout death/missing writes associated threat/loss without omniscient defender truth.

### Group returns with cargo

Verdict: NO for map ecology.

Shakedown goods surface exists elsewhere, but camp-map cargo/stockpile/structural bounty reduction is absent.

Needed test: cargo return updates camp stockpile and lead/cell bounty.

### Human camp moves or changes

Verdict: NO.

Needed test: old lead ages, new route/activity signal appears, threat only changes after recheck.

### Weather and concealment

Verdict: PARTIAL FOR SIGNAL GENERATION, NO FOR CAMP MAP POLICY.

Implemented: live smoke/light adapters use weather/time/exposure; rain/fog/portal storm/daylight/contained exposure influence projection.

Missing: results are not written as weather-shaped map leads; weather hiding approaching bandits from player/local actors is not proven.

Needed test: same fire under night/clear vs daylight/rain writes different confidence/range into camp map; approaching bandit visibility also affected where relevant.

### Cities and zombies

Verdict: NO for map ecology.

Needed test: city edge can be both bounty and threat; zombie pressure raises route danger/opportunity without requiring full tactical sim.

### Multiple camps see same target

Verdict: PARTIAL.

Implemented: per-site maps are independent and lead ids include site id.

Missing: no territoriality/distance/harvested/threat dogpile damping beyond global active player pressure cap and same-camp active group blocking.

Needed test: two camps can independently remember same target; same-camp dogpile blocked; multi-camp pressure damped by distance/threat/active pressure.

### Save during local contact

Verdict: PARTIAL.

Implemented: active group/member fields and ordinary NPC save path exist; tests cover active sortie timestamps.

Missing: no lead-bound active group join key/cargo/wounds/panic; no explicit reconnect/collapse interrupted result policy.

Needed test: save during local contact reloads without duplicating/losing group and can reconnect or honestly collapse to interrupted/unknown.

## Immediate recommendations

1. Fix migration semantics before more behavior depends on it: distinguish “missing `intelligence_map`” from “present but empty map.”
2. Add `lead_id` to active outing state and entry/return handoff before expanding dispatch. Otherwise the new map remains a side ledger.
3. Change `record_live_signal_mark()` or add wrappers so smoke/light writes real `camp_map_lead`s, while keeping old scalar fields only as compatibility/report mirrors.
4. Add map-driven dispatch selection for at least one path: choose a `smoke_signal`/`light_signal`/`basecamp_activity` lead, produce scout intent, carry `lead_id`, write scout result back.
5. Only then add structural cells/routes/frontier cleanup. Otherwise those systems will have nowhere honest to plug in.

## Final audit read

This commit should be treated as **foundation green, ecology incomplete**.

It makes the camp map persist and proves the first scout-return memory mark. It does not yet make the player feel the full source-of-truth fantasy: “I am sharing the region with another hungry intelligence whose map learns my light, smoke, routes, stockpiles, patterns, pain, and silence.”
