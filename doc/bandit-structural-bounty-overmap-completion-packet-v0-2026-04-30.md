# Bandit structural bounty overmap completion packet v0

## Classification

Proposed classification: **ACTIVE / GREENLIT implementation packet**.

Imagination source: `doc/bandit-structural-bounty-overmap-completion-imagination-source-of-truth-2026-04-30.md`.

Testing ladder: `doc/bandit-structural-bounty-overmap-testing-ladder-v0-2026-04-30.md`.

Existing canon anchors:

- `doc/bandit-overmap-ai-concept-2026-04-19.md`
- `doc/bandit-camp-map-ecology-source-of-truth-2026-04-28.md`
- `doc/bandit-camp-map-ecology-implementation-map-2026-04-28.md`
- `doc/bandit-camp-map-risk-reward-dispatch-planning-packet-v0-2026-04-28.md`
- `doc/bandit-live-product-playtest-matrix-v0-2026-04-28.md`

## Goal

Complete the live bandit overmap ecology so camps can act on ordinary structural OMT bounty, not only player-near smoke/light/direct-range triggers.

Bandit camps should maintain a sparse local operational map, seed low/medium OMT bounty from forests/towns/buildings, dispatch small parties toward those leads, reveal threat at stalking distance, subtract discovered threat from effective bounty/interest, consume structural bounty only if the party continues to arrival, and debounce repeated visits through harvested/dangerous/recently-checked memory.

## Scope

Implement:

1. Bounded per-camp OMT structural bounty scanning.
2. Coarse structural classification for forest/town/building/open terrain.
3. Camp-map lead seeding for structural bounty.
4. Non-player structural dispatch planning.
5. Abstract structural outing travel/stalking/arrival resolution.
6. Stalking-distance threat reveal and effective-interest writeback.
7. Structural bounty consumption on arrival only after interest survives threat.
8. Debounce for harvested, dangerous, recently checked, false-lead, and active-outside cases.
9. Save/load persistence for structural leads and active structural outings.
10. Deterministic multi-turn tests for no-stuck/no-repeat behavior.
11. Performance guardrails and metrics.
12. One live/harness product proof that an idle camp dispatches to forest/town structural bounty without player bait.

## Non-goals

Do not include:

- writhing stalker implementation;
- new Basecamp economy;
- full local bubble combat for every structural outing;
- exact loot scanning or exact monster population truth at broad radius;
- global overmap-wide scans;
- permanent terrain upgrade from a player/NPC sighting;
- multi-camp grand strategy beyond existing dogpile caps;
- new public release packaging;
- tuning-heavy numeric perfection before the lifecycle is real.

## Current implementation footing

Already present and reusable:

- `site_record::intelligence_map` persists camp-local leads.
- `camp_map_lead` has `kind`, `status`, `target_id`, `omt`, `radius_omt`, `source_key`, `source_summary`, `first_seen_minutes`, `last_seen_minutes`, `last_checked_minutes`, `last_scouted_minutes`, `bounty`, `threat`, `confidence`, `threat_confirmed`, `target_alert`, `scout_seen`, `generated_by_this_camp_routine`, `prior_bandit_losses`, `prior_defender_losses`, `times_checked_empty`, `times_harvested`, and `last_outcome`.
- `camp_lead_kind` already includes `structural_bounty`, `harvested_site`, `moving_actor`, `route_activity`, `smoke_signal`, `light_signal`, `sound_signal`, `threat_memory`, `loss_site`, `false_lead`, and `frontier_probe`.
- `choose_camp_map_dispatch()` already compares bounty/confidence/pressure against threat/caution/reserve.
- `plan_site_dispatch_from_camp_map_lead()` already turns remembered leads into scout/stalk dispatch plans for player-oriented handoff.
- `do_turn.cpp` already observes player-near smoke/light and can write live signal marks to camp memory.
- deterministic playback already models forest/town and moving-carrier choices.

Missing implementation is the structural live loop: seed, choose, travel, stalk, reveal threat, recompute interest, either abort/mark danger or arrive/consume, persist, and prove.

## Data model

Prefer reusing `camp_map_lead` fields instead of creating a second bounty map.

### Lead identity

Structural lead id:

```text
<site_id>:structural_bounty:<omt.x>,<omt.y>,<omt.z>:<terrain_class>
```

Examples:

- `overmap_special:bandit_camp@140,52,0:structural_bounty:137,49,0:forest`
- `overmap_special:bandit_camp@140,52,0:structural_bounty:144,46,0:town`

### Lead kind/status

Use:

- `kind = structural_bounty` before harvest;
- `kind = harvested_site` only if kind-level distinction is needed after harvest;
- `status = suspected` after broad scan;
- `status = scout_confirmed` after stalking-distance low-threat check;
- `status = active` while a party is en route, stalking, or on-site;
- `status = harvested` after structural bounty is consumed at arrival;
- `status = dangerous` after stalking-distance threat makes effective interest unattractive;
- `status = stale` after old low-confidence memory cools.

### Additional fields

Current fields may be enough. Add only if tests/review show ambiguity:

```cpp
int original_bounty = 0;          // optional: report/debug only
int next_eligible_minutes = -1;   // optional: explicit cooldown if last_checked is awkward
bool structural_consumed = false; // optional: only if status=harvested is insufficient in saved review
```

Recommendation: first implementation should avoid save schema growth unless needed. `status`, `bounty = 0`, `times_harvested`, `last_checked_minutes`, and `last_outcome` should be enough.

## Structural OMT classifier

Add a deterministic adapter in `bandit_live_world` or a small sibling module:

```cpp
struct structural_bounty_read {
    std::string terrain_class;
    int bounty = 0;
    int confidence = 0;
    int latent_threat = 0;       // uncertainty, not confirmed threat
    int radius_omt = 0;
    bool eligible = false;
    std::string summary;
};

structural_bounty_read classify_structural_bounty_omt( const overmap &omap,
        const tripoint_abs_omt &omt );
```

Initial classification:

| OMT class | bounty | confidence | notes |
|---|---:|---:|---|
| open field / meadow / empty road | 0 | 0 | no structural lead |
| forest / woods / wetland forest | 1 | 1 | thin forage/shelter/fuel skim |
| isolated house / farm / cabin / scattered building | 2 | 1 | medium target, scout first if uncertain |
| town/building fringe | 2-3 | 1-2 | better value, possible hidden threat |
| dense city block | 3-4 | 1 | high value but high uncertainty/threat reveal risk |
| discovered camp/settlement | existing site/mobile logic | existing | do not double-count as anonymous terrain |

Classifier must be coarse. It must not inspect exact items or exact monsters at broad range.

## Camp-local scan cadence

Add a bounded maintenance pass:

```cpp
void advance_structural_bounty_scan( bandit_live_world::state &state, int now_minutes );
```

Per site:

- skip retired empty sites;
- skip sites without ready/home members unless just cooling/debouncing;
- skip or heavily limit while an active outside group exists;
- sample from a camp-local radius, not the whole overmap;
- update only a small number of OMTs per maintenance tick;
- make selection deterministic per site/time bucket so save/load does not jitter.

Suggested cadence:

- near ring: radius 4-8 OMT, every 30-60 in-game minutes, sample 2-4 candidates;
- mid/frontier ring: radius 9-16 OMT, every 6-12 in-game hours, sample 1-2 candidates;
- daily cleanup: cool stale weak leads, preserve harvested/dangerous memory.

Use existing `camp_intelligence_map` cadence fields where possible:

- `next_near_tick_minutes`
- `next_mid_tick_minutes`
- `next_far_tick_minutes`
- `next_frontier_tick_minutes`
- `last_daily_cleanup_minutes`

## Seeding rules

A scan may create or refresh a structural lead only when:

- classifier says `eligible` and `bounty > 0`;
- no existing `harvested` lead suppresses that OMT;
- no `dangerous` lead suppresses it within cooldown;
- it is not only evidence generated by this camp’s own routine dispatch;
- it is not an actor/mobile clue disguised as terrain.

Refresh behavior:

- suspected structural lead may refresh bounty/confidence from terrain;
- harvested structural lead stays bounty `0` and does not naturally regenerate;
- dangerous lead keeps threat sticky until later stalking-distance contradictory evidence;
- new player/NPC/smoke/light signal can create a separate mobile/site/corridor lead at same OMT without restoring structural bounty.

## Dispatch selection

Add a structural outing planner separate from player pursuit handoff:

```cpp
struct structural_outing_plan {
    bool valid = false;
    std::string site_id;
    std::string lead_id;
    tripoint_abs_omt target_omt;
    bandit_dry_run::job_template job = bandit_dry_run::job_template::hold_chill;
    std::vector<character_id> member_ids;
    int expected_arrival_minutes = -1;
    int expected_return_minutes = -1;
    std::vector<std::string> notes;
};

structural_outing_plan plan_structural_bounty_outing( const site_record &site,
        const camp_map_lead &lead, int now_minutes );
```

Intent mapping:

- low-confidence forest/town lead: `scout`;
- confirmed low-threat forest: `scavenge` if supported, otherwise `scout` with harvest result;
- town/building: scout before harvest unless confidence/risk is already good;
- dense city: scout only first, no raid/steal from anonymous structural lead;
- high mobile player/NPC lead: existing stalk/pressure path, not structural outing.

Important implementation decision:

Current handoff supports scout/stalk player pressure. Do not jam structural forest collection through the player pursuit handoff. Add a **non-player structural outing path** that can share active member selection and persistence but has its own abstract stalking-distance threat writeback and later arrival/harvest writeback.

## Active outing persistence

Reuse existing `site_record` active fields where possible:

- `active_group_id = site.site_id + "#structural"` or existing dispatch id;
- `active_target_id = lead.lead_id`;
- `active_target_omt = lead.omt`;
- `active_job_type = "scout"` / `"scavenge"`;
- `active_member_ids = selected members`;
- `active_sortie_started_minutes = now_minutes`.

Open question: if `active_group_id` format must stay compatible with pursuit handoff, use a separate suffix but keep debug reports explicit.

Save/load success requires:

- lead persists;
- active target persists;
- active members persist as outbound;
- expected arrival/return can be recomputed or persisted;
- after reload, resolver continues once and does not double-consume.

If expected arrival/return is not persisted, compute it deterministically from started time, site anchor, target OMT, and job kind.

## Abstract travel, stalking, and arrival resolver

Add:

```cpp
void advance_structural_bounty_outings( bandit_live_world::state &state, int now_minutes );
```

Lifecycle:

1. `suspected` lead selected.
2. Members marked outbound; lead status becomes `active`.
3. Travel time passes toward a stalking band outside the target OMT.
4. At stalking distance, threat is evaluated and written back before arrival.
5. Effective interest is recomputed: `effective_bounty = bounty - threat_pressure - caution/friction`.
6. If effective interest no longer clears the job threshold: `status = dangerous`, `stale`, or `hold_off`; `threat` and `threat_confirmed` are written; `last_outcome = "threat_revealed_lost_interest"`; the party returns without consuming the target.
7. If effective interest still clears the threshold, the party proceeds to arrival.
8. On arrival, structural bounty is consumed; `status = harvested`; `bounty = 0`; `times_harvested++`; `last_outcome = "harvested_structural_bounty"`.
9. Members return after return travel time, unless killed/wounded result is implemented later.
10. Active group clears; members become at_home; camp memory remains.

First pass should not simulate detailed loot cargo unless already cheap. The product claim is behavior, memory, and dispatch life, not itemized haul.

## Threat reveal

Threat must be revealed closer than bounty, specifically at stalking distance rather than on arrival. Arrival is for consuming the target only if the stalking-distance risk check still leaves enough effective interest.

Broad scan writes:

- `bounty`
- `confidence`
- maybe `latent_threat` in source summary only
- `threat = 0`
- `threat_confirmed = false`

Stalking-distance scout/check writes actual threat:

Threat sources for first implementation:

- urban/dense city uncertainty baseline;
- known nearby hostile overmap special / loss site;
- remembered prior bandit losses;
- local monster/horde pressure if cheaply available;
- recent player/basecamp active pressure if the lead is not purely structural;
- direct existing live signal threat if present.

Do not do expensive monster scans globally. For v0, a deterministic coarse threat function is better than a slow fake-precise one.

Possible output bands:

- `0`: no threat noticed;
- `1`: caution;
- `2`: meaningful risk;
- `3+`: hold/dangerous.

Decision rule:

```text
effective_interest = bounty - threat_pressure - caution/friction
```

If effective interest falls below the job threshold, the party loses interest, records the threat/debounce state, and returns without stepping onto or harvesting the OMT.

## Debounce rules

Harvested:

- structural bounty becomes `0`;
- status `harvested` blocks future structural dispatch;
- `times_harvested++` and `last_checked_minutes = now`.

Dangerous:

- status `dangerous` blocks repeats until long cooldown or new strong evidence;
- threat stays sticky;
- a later scout/changed situation can downgrade, but timer alone should not erase it.

Checked empty / false lead:

- status `stale` or `invalidated` with `times_checked_empty++`;
- suppress short-term repeats.

Active:

- no parallel dispatch to same lead;
- no site dogpile while active outside group exists unless future design allows multi-party pressure.

Own routine traffic:

- if a lead is generated only by this camp’s own outgoing/returning party, mark `generated_by_this_camp_routine = true` and do not let it refresh external suspicion.

## Performance design

Hard constraints:

- no full-overmap scan per turn;
- no per-camp all-OMT scan every minute;
- no live monster/loot exact scan at broad radius;
- no repeated JSON/save inflation during normal do_turn;
- no log spam in ordinary gameplay.

Performance tactics:

- use per-site cadence timestamps;
- scan at most `N` OMTs per camp per cadence tick;
- cap total structural scan work per global maintenance pass;
- deterministic sampling/ring cursor rather than random searching;
- cache or derive terrain class from overmap terrain/land-use cheaply;
- skip retired/empty sites;
- skip or reduce scans when a site already has active outside pressure;
- keep debug logging behind concise one-line summaries and/or existing debug level.

Metrics to log/test:

- sites considered;
- OMTs sampled;
- leads created/refreshed/skipped;
- skipped harvested/dangerous/recently checked counts;
- structural active outings;
- stalking-distance threat checks processed;
- resolver arrivals processed;
- max per-pass scan budget hit yes/no.

Performance proof target:

- deterministic unit test for scan cap;
- playback benchmark up to 500 turns with several camps and mixed terrain;
- live/harness wait proof with one/two/four camps showing bounded counters and no obvious wait slowdown.

## Save/load design

Save compatibility:

- reuse serialized `camp_intelligence_map.leads` and active fields first;
- add new fields only if impossible to prove from existing state;
- if adding fields, include default deserialize values for old saves.

Save/load tests:

- structural lead persists after save/load;
- active structural outing persists after save/load;
- arrival after reload consumes once, not twice;
- harvested lead stays harvested after reload;
- dangerous lead keeps threat and debounce after reload;
- old saves without structural fields load cleanly.

Saved evidence for live proof:

- saved site has structural lead before dispatch;
- saved site has active structural outing after dispatch;
- saved site has stalking-distance threat/interest result;
- saved site has harvested result after arrival if interest survived;
- active member state transitions out and back;
- no duplicate active groups after reload.

## Deterministic tests

Add tests mostly in `tests/bandit_live_world_test.cpp`; add playback tests where multi-turn selection clarity matters.

### Classifier tests

1. `bandit_structural_bounty_classifies_forest_low`
   - forest/woods/wetland forest -> bounty 1, confidence 1, eligible.

2. `bandit_structural_bounty_classifies_town_medium`
   - house/town/building OMT -> bounty 2-3, eligible.

3. `bandit_structural_bounty_rejects_open_ground`
   - open field/road -> no structural lead.

### Scan tests

4. `bandit_structural_scan_seeds_sparse_leads`
   - bounded camp-local sample creates leads, not a flood.

5. `bandit_structural_scan_respects_harvested_debounce`
   - harvested lead at same OMT is not restored by broad scan.

6. `bandit_structural_scan_respects_dangerous_debounce`
   - dangerous lead suppresses repeat until explicit cooldown/changed evidence.

7. `bandit_structural_scan_does_not_upgrade_mobile_actor_to_ground_bounty`
   - player/NPC lead in forest stays moving_actor/mobile, forest remains low/unchanged.

### Dispatch tests

8. `bandit_structural_dispatch_sends_one_member_to_forest`
   - low forest lead creates small outing.

9. `bandit_structural_dispatch_scouts_town_before_harvest`
   - town uncertainty chooses scout/check before harvest.

10. `bandit_structural_dispatch_blocks_parallel_same_site_pressure`
    - active outside group blocks another structural dispatch.

11. `bandit_structural_dispatch_holds_high_threat_low_reward`
    - known threat > bounty chooses hold/dangerous handling before dispatch.

### Resolver tests

12. `bandit_structural_outing_reveals_threat_at_stalking_distance`
    - broad scan threat 0; stalking-distance check writes threat before arrival.

13. `bandit_structural_outing_loses_interest_when_threat_exceeds_bounty`
    - threat subtracts from bounty; party returns without stepping onto or harvesting the OMT.

14. `bandit_structural_outing_consumes_bounty_on_arrival_after_interest_survives`
    - after stalking check passes and travel completes, bounty 0, status harvested, member returns.

15. `bandit_structural_outing_does_not_repeat_harvested_tile_500_turns`
    - multi-turn simulation proves no stuck loop.

16. `bandit_structural_outing_does_not_pingpong_between_dangerous_tiles`
    - dangerous debounce works across turns.

17. `bandit_structural_outing_save_load_continues_once`
    - active outing survives serialization and consumes once.

### Playback / benchmark tests

18. `bandit_playback_structural_forest_town_progression_500`
    - forest skim, town scout, harvested/dangerous outcomes, no endless pressure.

19. `bandit_playback_structural_multi_camp_budget_stays_bounded`
    - multiple camps, scan cap honored, no dogpile explosion.

20. `bandit_playback_mobile_bounty_overrides_structural_when_fresh`
    - player/NPC high mobile lead wins while current, but does not alter ground bounty.

## Live/harness proof plan

Create at least one v0 harness scenario after deterministic proof is green:

`bandit.structural_bounty_idle_camp_forest_town_mcw`

Footing:

- owned bandit camp with ready members;
- no player smoke/light bait;
- nearby known forest and town/building OMTs in scan radius;
- player far enough not to trigger direct-player range;
- stable save fixture.

Proof steps:

1. Startup/load green.
2. Audit saved initial camp/site/members.
3. Run bounded wait until structural scan cadence.
4. Capture debug artifact: structural scan created candidate(s).
5. Capture dispatch plan: structural lead selected, job/member count recorded.
6. Save/audit active outing state.
7. Run bounded wait until stalking-distance resolution.
8. Capture stalking artifact: threat check, effective-interest computation, and either continue/abort decision.
9. If interest survives, run bounded wait until arrival/harvest.
10. Capture resolver artifact: harvested result, or lost-interest/danger result if threat won.
11. Save/audit lead status/bounty/threat/times fields.
12. Run additional bounded wait to prove no immediate repeat.
13. Save/audit no duplicate active group and member state returned/settled.

Evidence class:

- feature-path live/harness proof, not deterministic-only;
- decisive artifacts are debug lines plus saved-state metadata;
- screenshots are optional unless local UI/bubble materialization is involved.

Possible second scenario after v0:

`bandit.structural_bounty_reload_resume_mcw`

- start with active structural outing saved mid-travel;
- reload fresh process;
- wait to stalking-distance check and, if applicable, arrival;
- prove once-only threat/harvest resolution.

## Ralph Wiggum review loop

Working name: **Ralph Wiggum** — inspired by the simple agentic coding loop idea, but deliberately narrowed for this C-AOL lane. This is not an unattended infinite loop and not another agent bureaucracy shrine, bitte. It is a bounded pre/post review loop that keeps Andi from implementing a technically correct but spiritually dead mechanism.

Run Ralph Wiggum around every phase or meaningful handoff:

1. **Specify** — state the intended player/world behavior and the exact proof rung.
2. **Implement** — make the smallest slice that can satisfy that rung.
3. **Test** — run the deterministic/build/harness gate for that rung.
4. **Review** — compare the actual diff/evidence against the lived bandit vision.
5. **Revise or stop** — revise if the mismatch is small; stop/park/escalate if the slice violates the vision, loops twice inconclusively, or lacks the right evidence class.

Minimum stop conditions:

- no blind retries after the same blocker;
- no claiming green from the wrong evidence class;
- no broad/destructive/external actions;
- no continuing if threat/bounty semantics drift from the source-of-truth rules.

During the review step, answer:

- What player/world behavior is this supposed to create?
- Which source-of-truth rule does it satisfy?
- Does it preserve the bounty-before-threat asymmetry?
- Does threat get revealed at stalking distance, not arrival?
- Does discovered threat subtract from effective bounty/interest?
- Can the bandits lose interest and turn back before stepping onto the tile?
- Does arrival only consume bounty after interest survives?
- What loop/debounce prevents repeated dumb interest?
- What deterministic test proves the seam?
- What live/harness evidence proves the game path, not just the helper?
- What would make the behavior feel omniscient, suicidal, idle, or mechanically fake?

Ralph Wiggum output should be short and attached to the handoff/checkpoint, not a parallel novel:

```text
Ralph Wiggum pass:
- Phase/rung:
- Vision behavior:
- Rule(s):
- Risk smell:
- Test/evidence required:
- Verdict: keep / revise / park
```

A phase should not be called done if Ralph Wiggum says the code passes tests but violates the lived picture. Tests are the lockpick; Ralph Wiggum checks whether we opened the right door.

## Implementation phases

### Phase 0 — canon packaging

- Patch `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` from this packet after Schani/Josef confirms the contract.
- Mark as current active target because `TODO.md` currently has no active execution target and Josef explicitly promoted it.

### Phase 1 — deterministic substrate

- Add structural classifier.
- Add lead id/upsert helpers.
- Add unit tests for terrain classes and debounce.
- No live game claims yet.

### Phase 2 — scan/seed cadence

- Add per-camp bounded scan pass.
- Use existing cadence fields where possible.
- Tests prove sparse seeding, cap behavior, and no harvested/dangerous refresh.

### Phase 3 — structural outing planner/resolver

- Add non-player structural outing plan.
- Reuse member selection/reserve logic where honest.
- Add abstract travel/stalking/arrival resolution.
- Tests prove stalking-distance threat reveal, threat-minus-bounty lost interest, harvest, return, active-outside blocking.

### Phase 4 — save/load and anti-loop

- Serialization tests for active outing and harvested/dangerous states.
- 500-turn deterministic playback for no stuck loops and no repeated same-tile stepping.
- Performance counters and scan cap tests.

### Phase 5 — live wiring

- [x] Wire scan and outing resolver into `do_turn` maintenance with strict cadence/budget.
- [x] Add concise debug/report artifacts.
- [x] Build current runtime.

### Phase 6 — live proof

- Add harness scenario for idle camp -> structural dispatch -> arrival -> consumption/debounce.
- Add reload-resume scenario if phase 4 touched active persistence in nontrivial ways.
- Run narrow deterministic tests, rebuild, live probe, saved-state audits.

### Phase 7 — tuning/readout

- Tune bounty/threat numeric bands after lifecycle is proven.
- Update docs with final numbers, evidence paths, and caveats.
- Keep any unproven local-bubble materialization as future work, not hidden scope.

## Build/test gates

Minimum gates before claiming deterministic green:

- `git diff --check`
- `make -j4 obj/bandit_live_world.o obj/do_turn.o tests/bandit_live_world_test.o tests/bandit_playback_test.o LINTJSON=0 ASTYLE=0`
- `make -j4 tests LINTJSON=0 ASTYLE=0`
- `./tests/cata_test "[bandit][live_world][structural_bounty]" --success`
- `./tests/cata_test "[bandit][playback][structural_bounty]" --success`

Minimum gates before live proof:

- `make -j4 TILES=1 cataclysm-tiles LINTJSON=0 ASTYLE=0`
- scenario JSON validation through `python3 -m json.tool` for new harness scenarios/fixtures where applicable;
- `python3 tools/openclaw_harness/proof_classification_unit_test.py` if classifier/report logic changes;
- one feature-path harness probe for structural dispatch;
- saved-state metadata audit for leads/member/active outing/result.

Optional broader gates if the code touches more central do_turn/save paths:

- `./tests/cata_test "[bandit_live_world]" --success`
- `./tests/cata_test "[bandit][live_world]" --success`
- performance matrix row with multi-camp structural scanning.

## Success state

This packet is complete only when all of these are true:

- [x] Structural OMT classifier exists and deterministic tests cover forest/town/open classes.
- [x] Per-camp bounded structural scan seeds sparse camp-map leads without global scanning.
- [x] Harvested/dangerous/recently-checked debounce prevents immediate repeat interest.
- [x] Non-player structural outing planner can send a small bandit dispatch to forest/town structural bounty.
- [x] Abstract outing resolver reveals threat at stalking distance, subtracts it from effective bounty/interest, and only consumes structural bounty on arrival if interest survives.
- [x] Player/NPC mobile bounty remains attached to actors/routes and does not permanently upgrade terrain.
- [x] Save/load preserves structural leads, active outings, harvested/dangerous outcomes, and member state.
- [x] Deterministic 500-turn tests prove bandits do not get stuck repeating the same harvested/dangerous tile.
- [x] Performance tests/counters prove scan/outing work is bounded for multi-camp scenarios.
- [x] Real `do_turn` maintenance invokes bounded structural scan/outing maintenance with concise report/debug output and a current-runtime tiles build.
- [ ] Live/harness feature-path proof shows an idle camp dispatching to structural forest/town bounty without player smoke/light/direct-range bait.
- [ ] Live/harness proof shows stalking-distance threat/interest writeback, optional later arrival harvest, and no immediate repeat of the consumed/dangerous target.
- [ ] Existing player smoke/light signal dispatch behavior still passes its relevant tests and is not regressed.
- [ ] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` match the final active/closed state.

## Open questions

1. Exact terrain classifier source: should v0 use land-use code where available, overmap terrain id/name heuristics, or a small explicit allowlist of OMT ids?
2. Town consumption granularity: does stepping onto one town/building OMT consume only that OMT, or should a building cluster lead cover radius 1?
3. Job label: should forest/town structural harvest use existing `scavenge`, or keep `scout` for v0 until a formal non-player `scavenge` resolver is proven?
4. Threat source v0: which cheap stalking-distance threat signals are acceptable initially: terrain class only, known zombie/horde pressure, hostile sites, prior losses, or a blend?
5. Return consequences: should v0 simply return members alive, or include bounded chance of wound/loss from high threat? Recommendation: no random wound/loss in v0; write threat/danger first, add casualties later only if deterministic.
6. Bubble materialization: if the player enters the target OMT while an abstract structural outing is active, should the group materialize now or remain abstract for v0? Recommendation: abstract-only in v0 unless existing active group path already handles it safely.
7. Resupply: should any structural bounty ever regenerate through world events? Recommendation: not in v0; only new mobile/activity signal creates separate interest.

## Handoff sketch for Andi

Implement `Bandit structural bounty overmap completion packet v0` as the next active C-AOL target.

Start with deterministic substrate, not live harness first. Reuse `camp_intelligence_map` and `camp_map_lead`. Add structural OMT classifier, bounded camp scan, structural outing planner/resolver, stalking-distance threat reveal, threat-minus-bounty interest loss, harvest debounce, save/load proof, 500-turn anti-loop playback, performance counters, then one feature-path harness proof. Do not implement writhing stalker, local bubble materialization, exact loot harvest, or random casualty economy in this packet.
