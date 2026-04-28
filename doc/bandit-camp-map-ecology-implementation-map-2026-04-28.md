# Bandit camp-map ecology implementation map (2026-04-28)

Status: CODE DESIGN / NO TEST PLAN IN THIS DOCUMENT

Source of truth: `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`.

This document translates that product picture into game code responsibilities. It intentionally does **not** define validation, harness scenarios, or proof gates. Those belong elsewhere. This is only the coding map: what must exist in the game so the mechanic can become stable.

## Implementation north star

The code should not start from “which NPC job do we dispatch?” It should start from:

> A bandit camp owns a saved, dirty, partial intelligence map. The map notices bounty before threat, sends eyes, stores what those eyes learn, eats finite loot, refreshes living-human opportunity, remembers danger, and expands outward when hungry and ignorant.

If a proposed code path does not feed that loop, it is probably decoration.

## Current substrate to keep, but not worship

Existing useful footing:

- `bandit_live_world::world_state` is already saved under `overmap_buffer.global_state.bandit_live_world` through the normal `dimension_data.gsav` path.
- `bandit_live_world::site_record` already represents hostile sites/camps with members, active group fields, remembered scalar fields, shakedown state, and empty-site retirement state.
- `plan_site_dispatch`, `apply_dispatch_plan`, `choose_local_gate_posture`, `choose_sight_avoid_reposition`, sortie clocks, return packets, and live-signal marks already give partial seams.
- Existing `remembered_target_or_mark`, `remembered_threat_estimate`, and `remembered_bounty_estimate` are useful migration/substrate fields, but they are not the camp map.

Required correction:

- The camp map must become an explicit owned object under each `site_record`, not a few loose global/scalar memories.
- All dispatch, scout, harvest, signal, and handoff code should read/write through that camp map.

## Core data model

### Add an owned camp intelligence map

Add something shaped like:

```cpp
struct camp_intelligence_map {
    int schema_version = 1;
    int last_daily_cleanup_minutes = -1;
    int next_near_tick_minutes = -1;
    int next_mid_tick_minutes = -1;
    int next_far_tick_minutes = -1;
    int next_frontier_tick_minutes = -1;
    int known_radius_omt = 0;
    int frontier_radius_omt = 0;
    std::vector<camp_map_lead> leads;
    std::vector<camp_map_cell> cells; // optional sparse structural/checked state
    std::vector<camp_route_memory> routes; // optional, for human movement corridors
};
```

Store it directly on `bandit_live_world::site_record`, for example:

```cpp
camp_intelligence_map intelligence_map;
```

Do not store one shared map on `world_state`. `world_state` may provide lookup helpers, but ownership must remain per camp.

### Camp map lead

A lead is a thing the camp can think about.

Suggested fields:

```cpp
enum class camp_lead_kind {
    structural_bounty,
    harvested_site,
    human_activity,
    basecamp_activity,
    moving_actor,
    route_activity,
    smoke_signal,
    light_signal,
    sound_signal,
    threat_memory,
    loss_site,
    false_lead,
    frontier_probe,
};

enum class camp_lead_status {
    suspected,
    scout_confirmed,
    active,
    harvested,
    stale,
    invalidated,
    dangerous,
};

struct camp_map_lead {
    std::string lead_id;
    camp_lead_kind kind;
    camp_lead_status status;
    tripoint_abs_omt omt;
    int radius_omt = 0;
    std::string source_key;
    std::string source_summary;
    int first_seen_minutes = -1;
    int last_seen_minutes = -1;
    int last_checked_minutes = -1;
    int last_scouted_minutes = -1;
    int bounty = 0;
    int threat = 0;
    int confidence = 0;
    int age_minutes = 0; // derived/reportable; persist only if useful
    bool threat_confirmed = false;
    bool target_alert = false;
    bool scout_seen = false;
    bool generated_by_this_camp_routine = false;
    int harvested_value = 0;
    int remaining_structural_bounty = 0;
    int refresh_pressure = 0;
    int prior_bandit_losses = 0;
    int prior_defender_losses = 0;
    int times_checked_empty = 0;
    int times_harvested = 0;
    std::string last_outcome;
};
```

Important rules:

- `lead_id` must be stable across save/load. Derive it from owning `site_id`, kind/source, and coarse OMT bucket, not from vector index.
- `bounty` is the camp's estimate of value, not exact item truth.
- `threat` is the camp's estimate of danger, not exact defender truth.
- `confidence` says how much the camp trusts the mark.
- `threat_confirmed` prevents scary marks from passively melting away.
- `generated_by_this_camp_routine` prevents self-scout/self-patrol churn from becoming fresh truth.

### Sparse cells / region memory

Leads are for actionable notes. Cells are for background known world state.

Add sparse per-camp cells only where needed:

```cpp
struct camp_map_cell {
    tripoint_abs_omt omt;
    int structural_bounty = 0;
    int remaining_structural_bounty = 0;
    int terrain_value = 0;
    int last_evaluated_minutes = -1;
    int last_harvested_minutes = -1;
    int times_checked_empty = 0;
    bool known_empty = false;
    bool harvested = false;
    bool no_path = false;
};
```

Use cells for finite ground bounty, harvested state, checked-empty dampening, no-path dampening, and frontier expansion. Do not make every OMT in the world a saved cell. Create cells only when the camp has looked, harvested, or formed a meaningful structural estimate.

### Route memory

Human movement should not smear into fake terrain bounty.

Use route memory for repeated travel:

```cpp
struct camp_route_memory {
    std::string route_id;
    tripoint_abs_omt from;
    tripoint_abs_omt to;
    int last_seen_minutes = -1;
    int confidence = 0;
    int bounty = 0;
    int threat = 0;
    int intercept_value = 0;
    int times_seen = 0;
    bool target_alert = false;
};
```

Roads and corridors become valuable because moving bounty passes through them, not because asphalt prints loot. Sehr sophisticated, the road remains a road.

## Save/load details

### Serialize with the camp

Add `serialize` / `deserialize` for:

- `camp_intelligence_map`
- `camp_map_lead`
- `camp_map_cell`
- `camp_route_memory`
- enum string conversion helpers

Then serialize `site_record::intelligence_map` inside `site_record::serialize` and read it in `site_record::deserialize`.

### Migration from old remembered fields

On deserialize, if no `intelligence_map` exists but any old scalar memory exists:

- create one `camp_map_lead` with kind `human_activity` or `frontier_probe` depending on available source;
- copy `remembered_target_or_mark` into `source_key` / `source_summary`;
- copy `remembered_bounty_estimate` into `bounty`;
- copy `remembered_threat_estimate` into `threat`;
- set confidence low-to-medium, not perfect;
- set status `suspected` unless the old state names a scout-confirmed return.

Keep the old fields only as compatibility/report mirrors until all live paths consume the new map. New behavior should not depend on them.

### Save during active outings and local handoff

The saved state must survive all of these cases:

- camp has no active group, only map memory;
- scout is out and has an associated `lead_id`;
- stalk/raid/toll group is out and has an associated `lead_id`;
- group has entered local play and carries a join key back to the abstract group/lead;
- group has cargo/wounds/panic but has not yet returned home;
- camp is being retired only if no home members and no outside/local/returning members exist.

Do not duplicate precise local NPC combat state in the camp map. Ordinary game save owns local NPC truth. The bandit map owns abstract memory, active group references, and return payload results.

## Camp sight and cadence

### Sight radius is camp-owned

Each site profile should expose camp-map perception knobs:

```cpp
struct camp_perception_profile {
    int near_radius_omt;
    int mid_radius_omt;
    int far_radius_omt;
    int frontier_max_radius_omt;
    int near_tick_minutes;
    int mid_tick_minutes;
    int far_tick_minutes;
    int frontier_tick_minutes;
};
```

The camp does not see all rings equally.

- Close rings refresh often.
- Far rings refresh slowly.
- Frontier expansion only happens under hunger/ignorance pressure.
- Event signals can create/update leads immediately, but dispatch choices still pass through cadence and resource gates.

### Distance-dependent cadence

Do not implement cadence as “every camp thinks about every mark every turn.” The camp should wake by distance band.

Suggested first shape:

- near ring: around the camp and active local theater, frequent enough to feel alive;
- mid ring: ordinary operating space;
- far ring: suspected signals and remembered edges;
- frontier ring: only for deliberate exploration.

For each cadence pass:

1. pick the due band(s);
2. update soft lead age/confidence for those bands;
3. ingest queued signals relevant to those bands;
4. score candidate leads for that band;
5. maybe dispatch one bounded outing if camp gates permit;
6. schedule the next tick for that band.

Distance changes attention frequency. It must not multiply travel budget. Movement remains time-earned, not free per wake.

### Daily cleanup

A daily cleanup should:

- remove weak stale soft signals;
- keep confirmed threat unless rechecked;
- keep harvested/checked/no-path dampening;
- collapse duplicate leads;
- cap total saved leads/cells per camp;
- preserve strong human/basecamp recurring activity marks;
- update hunger/stockpile pressure that drives exploration.

## Signal ingestion

Signals are not truth. They are lead writers.

### Signal adapter shape

Create helper(s) around current physical systems:

```cpp
std::vector<camp_signal_observation> collect_bandit_signal_observations(
    const site_record &site,
    const tripoint_abs_omt &band_or_origin,
    int current_minutes );
```

or event-driven writers such as:

```cpp
void record_smoke_signal_for_bandit_maps(...);
void record_light_signal_for_bandit_maps(...);
void record_human_activity_signal_for_bandit_maps(...);
```

The adapter writes per-camp leads. It should not be a global psychic signal list that every camp reads identically.

### Smoke and light

Smoke/light usually create bounty, not immediate precise threat.

Code rules:

- smoke creates `smoke_signal` leads with bounty/confidence shaped by plume strength, wind, rain, shelter, distance, and duration;
- ordinary visible light creates `light_signal` leads with bounty/confidence shaped by darkness/daylight, containment, direction/exposure, weather, terrain, and distance;
- daylight suppresses distant light usefulness hard;
- contained indoor light should leak directionally/coarsely rather than omnidirectionally;
- smoke/light should not name exact inventory, exact player identity, or exact defender count;
- swivel/sweeping lights are excluded from the ordinary “static site bounty” rule unless a later explicit vehicle/convoy signal path handles them.

### Human/basecamp signals

Human signals create richer leads:

- direct human/NPC sightings;
- repeated route use;
- camp routines;
- vehicle/cargo movement;
- basecamp work, smoke, light, storage, NPC movement.

These leads can refresh because humans keep producing value. They should be source-shaped: a moving actor, a route, or a living site, not a fake structural bounty smeared onto the dirt.

### Threat signals

Threat writes require closer or harder evidence:

- scout sighting of armed defenders;
- local contact;
- bandit injury/death;
- traps/fortification observation;
- serious zombie pressure on route/site;
- failed extraction or panic return.

Threat may have a smaller effective observation radius than bounty. It should be raised from contact and lowered only by real recheck or successful safe passage.

## Bounty lifecycle

### Structural bounty

Structural bounty is finite place-value.

Code responsibilities:

- estimate coarse value by overmap terrain/site class;
- create/update sparse `camp_map_cell` or `structural_bounty` lead;
- when bandits exploit it abstractly, reduce `remaining_structural_bounty` and mark harvested;
- do not regenerate it passively;
- allow new activity to create a new mobile/signal lead without restoring the harvested structural bucket.

If the target is unloaded, use abstract value buckets. If the target is loaded and the encounter actually loots visible items, local item removal belongs to ordinary map/NPC logic. Do not double-count: either abstract harvest resolves the offscreen bucket, or local loot is removed by the local encounter and the return packet reports cargo.

### Mobile and human bounty

Mobile/human bounty refreshes because people act.

Code responsibilities:

- keep leads attached to source identity when possible: player/basecamp/activity/site/route/group;
- refresh from repeated smoke/light/routes/work instead of passive timer magic;
- clear, split, or rewrite if the source moves, disperses, is disproved, or the outing leash expires;
- avoid immortal pursuit memory.

### Harvest result

When a bandit group returns with loot:

- apply cargo to camp stockpile/bounty ledger;
- reduce the relevant lead/cell bounty;
- mark source as harvested if structural;
- mark source as still-living/refreshable if human/basecamp;
- write `last_outcome` and `last_checked_minutes`;
- reduce appetite for immediate repeat exploitation.

## Threat lifecycle

Threat is scar tissue.

Code responsibilities:

- store threat on leads/cells/routes with `threat_confirmed` when learned by sight/contact/outcome;
- do not passively decay confirmed threat;
- allow soft suspected danger to lose confidence;
- downgrade confirmed threat only through close contradictory observation, safe passage, or a return packet that explicitly says the danger was lower than expected;
- strengthen threat from losses, wounds, panic, heavy zombies, bad retreat geometry, or obvious defenders;
- make threat affect intent/risk but not automatically produce bigger attacks.

## Exploration when the cupboard is empty

If no useful lead beats `hold/chill` and the camp is hungry or low on bounty, the camp should expand its map deliberately.

Code a distinct `frontier_probe` / `explore` path:

- choose a frontier arc/ring just beyond known useful space;
- prefer roads, building clusters, city edges, water/forest resource margins, and previously unscouted structural value;
- avoid known high-threat/no-path/harvested-only zones unless desperation says otherwise;
- send only a bounded scout/explore outing;
- write checked-empty, structural-bounty, no-path, smoke/light/human, or threat leads from what the scout finds;
- return home on leash expiry;
- do not let path failure become random wandering.

Exploration is hunger drawing circles, not the engine shrugging and picking a random goal. Important distinction, because otherwise the machine becomes soup with boots.

## Dispatch and camp ledger

### Camp ledger inputs

The dispatch code needs real camp state, even if first values are coarse:

- living roster;
- ready-at-home roster;
- wounded/unready members;
- active outside/local/returning groups;
- hard home reserve;
- stockpile/bounty pressure;
- recent gains;
- recent losses;
- current confidence/aggression/caution modifiers;
- per-profile limits.

If detailed food/ammo/med stockpiles are not available yet, add explicit bounded placeholder fields rather than hiding need pressure inside magic constants.

### Intent before count

Choose intent from the map first:

- hold/chill;
- explore/frontier probe;
- scout;
- re-scout;
- scavenge/harvest;
- stalk/pressure;
- toll/shakedown;
- steal;
- raid;
- return/stale.

Then size the group from intent, roster, reserve, and active group gates. Do not start by picking “send N bandits” and then invent why.

### Active group blocking

Same-camp same-target dogpiles should fail closed:

- if an active group is outbound/local/returning for a lead, do not dispatch another same-target group;
- allow one active pressure per camp/lead unless a later explicit reinforcement rule says otherwise;
- multiple camps may accidentally notice the same target, but they do not share maps or coordinate as allies in v1.

### Other bandit camps

Other bandit camps should generally write as threat-bearing sites, not friendly shared assets.

Possible treatment:

- known other camp -> low/zero bounty, meaningful threat, territorial dampening;
- unknown other camp encountered by scout -> dangerous site lead;
- no shared intelligence map;
- no coalition planner.

## Active overmap groups

Current `site_record` active fields are too flat for the full mechanic. Add a durable active group structure or expand the existing handoff group state so each outing carries:

- `group_id`;
- `source_site_id`;
- `lead_id`;
- intent/job;
- member ids or abstract strength;
- route/target OMT;
- current posture;
- start time;
- return clock;
- time-earned travel budget;
- cargo/burden;
- wounds/panic/losses;
- local handoff join key;
- last report summary.

The group should be able to be saved while outbound, local, returning, or collapsed back.

## Scout behavior

### Ordinary bounty scout

For a suspected ordinary loot/site lead:

1. camp notices broad bounty;
2. scout/explore group goes near enough to confirm;
3. scout writes bounty/threat/confidence/check result;
4. if worth exploiting and safe enough, camp dispatches scavenge/steal;
5. returned cargo reduces finite bounty.

### Human/basecamp scout

For human/basecamp activity:

1. camp notices bounty/signal before threat;
2. scout approaches to observation envelope;
3. default camp/basecamp observation stand-off is two OMT from target OMT when pathing/terrain supports it;
4. scout watches for bounded time, roughly half an in-game day for the current planning lane;
5. scout returns and writes a lead with bounty, threat, confidence, target-alert/scout-seen, source/outcome;
6. later cadence chooses hold/re-scout/stalk/toll/raid from that map lead.

### Scout seen / sight avoidance

If a scout/stalker becomes visible in local play:

- local code increments exposure state;
- immediate reposition should be attempted if legal;
- by two visible local turns, the bandit must have moved, begun abort/return, or reported blocked/no-cover;
- no teleporting;
- repeated exposure writes `scout_seen` / `target_alert` back to the lead;
- target-alert raises risk and can downgrade raid pressure into stalk/re-scout/hold.

## Overmap-to-bubble handoff

The seam must be bidirectional.

### Entry payload

When an abstract group becomes local, build payload from the camp map and active group:

- source camp id;
- group id;
- lead id;
- intent/job;
- group strength/member ids;
- target OMT or route segment;
- bounty/threat/confidence as known by the camp;
- scout_seen/target_alert;
- return clock;
- cargo capacity;
- retreat bias;
- goal stickiness;
- anchored identities if any.

The local side gets what the bandits believe, not exact truth.

### Local behavior boundary

Local map AI should handle:

- movement/pathing;
- actual sight;
- cover/repositioning;
- contact/search/observe/loot/retreat states;
- combat or shakedown surface when appropriate.

Local map AI should not invent a new strategic mission unrelated to the source lead. It may continue, probe, shadow, divert to an immediate same-route opportunity, abort, or return within the same outing leash.

### Return packet

When the encounter cools, the group exits, or the local result resolves, build a return packet:

- survivors/strength;
- killed/missing/wounded members;
- cargo acquired;
- defender losses observed;
- bandit losses;
- panic/morale;
- updated bounty;
- updated threat;
- new marks;
- target-alert/scout-seen;
- outcome: harvested / empty / too dangerous / still active / paid toll / failed / no path / returned.

Apply the packet to:

- camp ledger;
- active group state;
- member states;
- camp map lead/cell/route;
- stockpile/need pressure;
- threat/bounty memories.

Then collapse, return home, or retire the group explicitly.

## Edge cases that need code behavior

### Nothing visible / no useful leads

- If camp stockpile/bounty pressure is low: hold/chill.
- If camp is hungry/low bounty and map is empty: bounded frontier exploration.
- If map has only stale soft leads: re-scout or explore, not raid.
- If map has only confirmed high threat: avoid or scout cautiously unless desperate.

### Signal vanishes

- Vanished smoke/light weakens soft confidence.
- A scout-confirmed human/basecamp lead remains actionable from memory.
- A vanished ordinary structural lead remains as a place to check/harvest if not already disproved.

### False lead / empty check

- Write `false_lead` or checked-empty dampening.
- Lower confidence and suppress immediate re-dispatch.
- Keep enough memory to avoid repeated idiocy.

### No path

- Mark lead/cell no-path or route-blocked with timestamp.
- Try another valid lead or hold.
- Do not fall back to random wandering.

### Camp has no home members

- If active outside/local/returning members exist, do not retire.
- If no home members and no outside/local/returning members exist, retire/ignore per empty-site cleanup.

### Camp has active outside group

- Block same-target dogpile.
- Allow unrelated low-risk home-side behavior only if reserve and code explicitly supports it; v1 can simply block outward dispatch until resolution.

### Scout dies

- Mark member dead/missing.
- Increase threat/caution on associated lead if death location/source is known.
- Do not magically learn full target truth unless local result packet honestly observed it.

### Group returns with cargo

- Apply stockpile/bounty gain.
- Reduce harvested structural bounty.
- If human target paid/surrendered, abstract the goods into camp gain without requiring exact item persistence unless local scene already owns exact items.

### Human camp moves or changes

- Old lead remains but confidence ages.
- New route/activity signals create or refresh mobile/human leads.
- Threat downgrades only after recheck, not because the player left the screen.

### Weather and concealment

- Weather can reduce/fuzz smoke/light/human signal confidence.
- Same weather also hides approaching bandits from the player/local actors when relevant.
- Wind can displace/fuzz smoke source quality.
- Rain reduces fire/smoke/light relevance where physical systems support it.
- Darkness boosts light usefulness; daylight suppresses it.

### Cities and zombies

- Cities can be bounty and threat at once.
- Zombie pressure increases route/local danger but can also create opportunistic defender-distraction openings.
- Do not implement full bandit-vs-zombie tactical sim as part of this map foundation.

### Multiple camps see same target

- Each camp writes its own lead independently.
- No shared map.
- Distance, territoriality, active pressure, harvested state, and threat should damp routine multi-camp dogpiles.

### Save during local contact

- Ordinary game save stores local NPC state.
- Bandit active group stores join key and abstract mission state.
- On reload, reconnect if possible; otherwise collapse with an honest interrupted/unknown result rather than duplicating or losing the group.

## Code integration points

Likely first files/seams:

- `src/bandit_live_world.h/.cpp`: data model, serialization, map update helpers, dispatch inputs, return packet application.
- `src/overmapbuffer.h` and global state save path: no new global map ownership; only ensure nested site map serializes through existing global state.
- `src/do_turn.cpp`: strategic cadence hook for owned hostile sites, dispatch evaluation, live signal consumption, return-home handling.
- smoke/light/sound producers: write signal observations into camp maps through a narrow adapter, not scattered direct mutations.
- local NPC / map AI seam: entry payload consumption, scout/stalker observation posture, sight avoidance, shakedown/raid/probe behavior, return packet creation.
- debug/report helpers: render camp map leads, cadence decisions, chosen intent, reserve, dispatch size, signal source, lead id, and outcome.

## Cradle-to-stable coding sequence

1. Add the saved per-camp map structs and enum conversion.
2. Add serialization/deserialization and old-field migration.
3. Add helpers to create/update/dedupe leads by stable id.
4. Add sparse structural bounty/cell estimation from OMT/site class.
5. Add signal adapter writers for smoke/light/human/activity leads.
6. Add distance-band cadence scheduling on each camp map.
7. Add lead aging, soft decay, checked-empty, harvested, and confirmed-threat rules.
8. Add frontier exploration intent when map is empty and camp need is high.
9. Add roster/reserve/stockpile ledger inputs to dispatch scoring.
10. Change dispatch to select intent from map leads before member count.
11. Add durable active group state tied to `lead_id`.
12. Wire scout return to write camp-map leads, not just scalar remembered fields.
13. Wire ordinary harvest/scavenge return to reduce finite structural bounty.
14. Wire human/basecamp activity to refresh mobile/site bounty.
15. Wire threat recheck/update only from close observation/local outcome.
16. Wire overmap-to-bubble entry payload from active group + lead.
17. Wire local scout/stalker posture, sight avoidance, and target-alert writeback.
18. Wire bubble-to-overmap return packet into camp map, camp ledger, and group/member state.
19. Add cleanup/caps so old low-value map clutter does not grow forever.
20. Add readable reports so every decision can be inspected without spelunking the save file.

The stable endpoint is not “a bandit got dispatched.” The stable endpoint is: a saved camp-owned map notices value, sends eyes, learns, eats finite bounty, refreshes living-human opportunity, remembers threat, expands when hungry, and carries consequences across overmap/local seams.
