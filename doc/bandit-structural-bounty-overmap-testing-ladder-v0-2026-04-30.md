# Bandit structural bounty overmap testing ladder v0

Parent packet: `doc/bandit-structural-bounty-overmap-completion-packet-v0-2026-04-30.md`

Vision anchor: bandit camps should make sparse local overmap decisions from structural bounty, reveal threat only at stalking distance, subtract threat from effective interest, turn back when the target stops being worth it, and consume structural bounty only after they still choose to arrive.

## Testing principle

Do not test this as one heroic end-to-end soup.

Test the feature as a ladder where each rung proves one missing seam:

1. **Map truth** — what OMT class is this?
2. **Memory truth** — what lead did the camp write?
3. **Choice truth** — why did this lead win or lose?
4. **Stalking truth** — what threat was revealed before arrival?
5. **Interest truth** — did threat subtract from bounty correctly?
6. **Outcome truth** — did the party turn back or arrive/harvest?
7. **Persistence truth** — does save/load preserve the exact state?
8. **Loop truth** — do 500 turns avoid repeat nonsense?
9. **Live truth** — does the real game path do the same thing without player bait?
10. **Performance truth** — does waiting stay bounded with multiple camps?

Each rung needs its own evidence class. A unit test cannot impersonate live behavior; a live screenshot cannot impersonate serialization proof; a green helper cannot impersonate product-path deployment. Ja eh, the usual paperwork goblin.

## Ralph Wiggum before/after loop

Ralph Wiggum here means a bounded agentic coding loop applied to each proof rung: specify, implement, test, review, revise-or-stop. It is useful precisely because it is simple; it becomes stupid the moment it turns into blind infinite retries.

Run Ralph Wiggum before and after every phase:

```text
Ralph Wiggum pass:
- Phase/rung:
- Plan element:
- Intended player/world behavior:
- Source-of-truth rule(s):
- Bounty/threat asymmetry check:
- Stalking-distance threat check:
- Threat-minus-bounty interest check:
- Turn-back-before-arrival check:
- Arrival-consumes-only-after-interest-survives check:
- Evidence class required:
- Deterministic proof:
- Live/harness proof:
- Stop condition hit?
- Verdict: keep / revise / park
```

A slice is not done if Ralph Wiggum says it passes tests but violates the lived bandit picture. A slice also stops if two attempts hit the same evidence blocker without materially changing setup, instrumentation, or hypothesis.

## Rung 1 — OMT classifier tests

Evidence class: deterministic contract.

Purpose: prove the classifier is boring, cheap, and honest.

Tests:

- `bandit_structural_bounty_classifies_forest_low`
  - forest/woods/wetland forest -> low bounty, low confidence, eligible.
- `bandit_structural_bounty_classifies_town_medium`
  - house/farm/town/building OMT -> medium bounty, eligible.
- `bandit_structural_bounty_rejects_open_ground`
  - open field/meadow/road/empty -> no structural lead.
- `bandit_structural_bounty_does_not_peek_exact_loot_or_monsters`
  - broad classifier depends on OMT class, not exact tile loot or exact creature positions.

Ralph Wiggum smell:

- If the classifier knows exact monsters/items at broad range, it is too omniscient.
- If forest equals city value, the ecology is fake.

## Rung 2 — camp memory seeding tests

Evidence class: deterministic contract plus saved-state serialization where fields are touched.

Purpose: prove broad bounty can enter camp memory without waking the whole map.

Tests:

- `bandit_structural_scan_seeds_sparse_leads`
  - one camp, mixed terrain, bounded sample count, sparse leads.
- `bandit_structural_scan_respects_scan_budget`
  - several camps, cap hit is visible and bounded.
- `bandit_structural_scan_skips_retired_empty_sites`
  - retired camp does not scan.
- `bandit_structural_scan_does_not_refresh_harvested_bounty`
  - harvested structural lead remains bounty 0.
- `bandit_structural_scan_does_not_refresh_dangerous_interest`
  - dangerous/threat-marked target does not immediately become attractive again.
- `bandit_structural_scan_does_not_convert_mobile_actor_to_ground_bounty`
  - player/NPC high mobile bounty stays actor/route lead, not terrain upgrade.

Required fields to inspect:

- `kind`
- `status`
- `bounty`
- `threat`
- `confidence`
- `last_seen_minutes`
- `last_checked_minutes`
- `times_harvested`
- `last_outcome`
- `generated_by_this_camp_routine`

Ralph Wiggum smell:

- If camp routine traffic refreshes its own leads, the bandits become a self-licking ice cream cone. Technical term, tragically.

## Rung 3 — dispatch selection tests

Evidence class: deterministic contract.

Purpose: prove the camp can choose structural work without player bait and without dogpiling.

Tests:

- `bandit_structural_dispatch_sends_small_party_to_forest`
  - low forest lead chooses small scout/scavenge outing.
- `bandit_structural_dispatch_prefers_town_when_value_justifies_risk`
  - medium town/building lead can outrank forest when threat is still unknown/low.
- `bandit_structural_dispatch_blocks_parallel_same_site_pressure`
  - already-active outside group blocks a second structural dispatch.
- `bandit_structural_dispatch_ignores_recently_checked_low_interest_target`
  - debounce prevents repeat visits.
- `bandit_structural_dispatch_known_threat_subtracts_interest`
  - known threat reduces effective interest before dispatch.

Expected debug/report shape:

```text
candidate=<lead_id> bounty=<n> known_threat=<n> confidence=<n> effective_interest=<n> decision=<job|hold>
```

Ralph Wiggum smell:

- If dispatch chooses a target with `effective_interest <= threshold`, it is suicidal bookkeeping.
- If dispatch never chooses structural leads without player smoke/light, the ecology is still asleep.

## Rung 4 — stalking-distance threat reveal tests

Evidence class: deterministic contract.

Purpose: prove threat appears before arrival and before harvest.

Tests:

- `bandit_structural_outing_reveals_threat_at_stalking_distance`
  - broad scan threat 0; stalking check writes actual threat before arrival.
- `bandit_structural_outing_stalking_check_does_not_consume_bounty`
  - stalking-distance threat reveal leaves structural bounty intact unless later arrival happens.
- `bandit_structural_outing_records_threat_source_summary`
  - debug/source summary explains why threat appeared.
- `bandit_structural_outing_stalking_state_survives_save_load`
  - save mid-stalk, reload, continue without double reveal.

Required state distinction:

- before stalking: `threat = 0`, `threat_confirmed = false`;
- after stalking reveal: `threat > 0`, `threat_confirmed = true`, `bounty` unchanged;
- after abort: no harvest/consume;
- after later arrival: harvest/consume only if interest survived.

Ralph Wiggum smell:

- If threat is first written at arrival, this rung fails even if the final status looks plausible.

## Rung 5 — threat-minus-bounty interest tests

Evidence class: deterministic contract.

Purpose: prove discovered threat subtracts from effective bounty and can make the bandits turn back.

Tests:

- `bandit_structural_outing_loses_interest_when_threat_exceeds_bounty`
  - low bounty forest + meaningful threat -> return, no harvest.
- `bandit_structural_outing_continues_when_bounty_exceeds_threat`
  - medium town/building + low threat -> proceed to arrival.
- `bandit_structural_outing_caution_friction_can_break_ties`
  - borderline target does not become a repeat magnet.
- `bandit_structural_outing_lost_interest_sets_debounce`
  - lost-interest target is not immediately reselected.

Decision formula under test:

```text
effective_interest = bounty - threat_pressure - caution/friction
```

The exact numeric threshold may tune later, but the sign and ordering must be deterministic.

Ralph Wiggum smell:

- If threat is merely a label and does not change choice, it is decorative UI compost.

## Rung 6 — arrival/harvest tests

Evidence class: deterministic contract plus save/load.

Purpose: prove arrival is harvest/writeback, not the first risk check.

Tests:

- `bandit_structural_outing_consumes_bounty_on_arrival_after_interest_survives`
  - passed stalking check -> arrival -> bounty 0, status harvested, `times_harvested++`.
- `bandit_structural_outing_arrival_is_once_only_after_reload`
  - save mid-travel-after-stalk, reload, arrival consumes once.
- `bandit_structural_outing_harvested_tile_does_not_regenerate_from_scan`
  - later scan does not restore structural bounty.

Required state:

- `status = harvested`
- `bounty = 0`
- `times_harvested` incremented once
- `last_outcome = harvested_structural_bounty`
- member/outbound state clears/returns

Ralph Wiggum smell:

- If arrival can happen without a prior stalking-distance interest check, the model collapsed.

## Rung 7 — 500-turn anti-loop playback

Evidence class: deterministic multi-turn simulation / benchmark.

Purpose: prove the ecology does not create a churn machine.

Tests:

- `bandit_playback_structural_forest_town_progression_500`
  - forest skim, town stalk, threat/harvest outcome, no repeated same-target loop.
- `bandit_playback_structural_danger_debounce_500`
  - danger/lost-interest target is not spammed.
- `bandit_playback_structural_harvest_debounce_500`
  - harvested target remains spent.
- `bandit_playback_mobile_bounty_overrides_structural_when_fresh`
  - fresh player/NPC lead can outrank structural work without permanently upgrading ground bounty.
- `bandit_playback_structural_multi_camp_budget_stays_bounded`
  - several camps, scan/outing counters stay under cap.

Required counters:

- leads seeded
- dispatches planned
- stalking checks processed
- lost-interest returns
- arrivals/harvests processed
- skipped harvested/dangerous/recently checked
- scan budget hits
- max per-turn/per-pass work

Ralph Wiggum smell:

- If the test asserts only final status and not the path/counters, it can miss the actual loop disease.

## Rung 8 — live/harness structural dispatch proof

Evidence class: feature-path live/harness proof.

Scenario name proposal:

`bandit.structural_bounty_idle_camp_forest_town_mcw`

Purpose: prove the real game path can create and act on structural leads without player smoke/light/direct-range bait.

Fixture footing:

- owned bandit camp with ready members;
- nearby forest and town/building OMTs in scan radius;
- no player smoke/light lure;
- player far enough not to be the cause;
- stable save fixture;
- debug artifact capture enabled for camp scan/dispatch/stalk/arrival.

Probe ledger:

1. Startup/load green.
2. Saved-state audit: site exists, members ready, no active outside group.
3. Wait via real long-wait primitive, not dot spam.
4. Artifact: structural scan seeded forest/town lead.
5. Artifact: structural dispatch selected a lead and member(s).
6. Saved-state audit: active outing target/member state persisted.
7. Wait to stalking-distance check.
8. Artifact: threat reveal + effective-interest computation.
9. Branch A: lost interest -> return without harvest; saved state proves bounty remains and debounce/threat exists.
10. Branch B: interest survives -> wait to arrival; artifact/saved state proves harvest consumed bounty once.
11. Additional wait proves no immediate repeat.
12. Cleanup/report path records artifact directory.

Minimum proof packet:

```text
claim:
run:
screen: startup/load only unless visible overmap state is actually inspected
tests:
artifacts/logs:
state audit:
verdict:
```

Ralph Wiggum smell:

- If the run only proves startup/load, it is not structural bounty proof.
- If the player is the lure, it is not idle structural bounty proof.
- If wait elapsed time is not proven, the probe is yellow.

## Rung 9 — reload-resume live proof

Evidence class: feature-path save/load continuity.

Scenario name proposal:

`bandit.structural_bounty_reload_resume_mcw`

Purpose: prove active structural outings survive process/save continuity.

Branches:

- save before stalking-distance threat reveal;
- save after stalking reveal but before return/arrival;
- save after arrival/harvest.

Required proof:

- reload sees same active target/member state;
- threat reveal does not double-apply;
- harvest does not double-consume;
- lost-interest debounce survives;
- active group clears once.

Ralph Wiggum smell:

- If reload “fixes” the state by losing it, the test is red, not a harmless reset.

## Rung 10 — performance proof

Evidence class: deterministic benchmark plus live wait sanity.

Purpose: prove this does not tank wait/pass-time and frame rate.

Deterministic proof:

- multi-camp simulation with mixed forest/town/open terrain;
- scan budget cap enforced;
- 500 turns with counters and max work per pass;
- no all-overmap scan.

Live proof:

- one-camp baseline wait;
- two/four-camp stress wait if fixture work is cheap enough;
- record elapsed game time, real command path, and whether wait was interrupted;
- capture structural counters before/after.

Required metrics:

- wall-clock rough duration for bounded wait/probe;
- camps considered;
- OMTs sampled;
- scan cap hit yes/no;
- active outings count;
- debug log line count bounded/no spam.

Ralph Wiggum smell:

- If the design works but wait feels like stirring cold tar, it is not done.

## Closure gates

Do not close the packet until these are all true:

- [ ] classifier tests green;
- [ ] memory seeding/debounce tests green;
- [ ] dispatch selection tests green;
- [ ] stalking-distance threat reveal tests green;
- [ ] threat-minus-bounty interest tests green;
- [ ] arrival-after-interest harvest tests green;
- [ ] save/load active outing tests green;
- [ ] 500-turn anti-loop playback green;
- [ ] multi-camp budget/perf proof green;
- [ ] live idle-camp structural proof green or explicitly handed to Josef as manual playtest package;
- [ ] Ralph Wiggum before/after notes exist for every implementation phase;
- [ ] `Plan.md`, `TODO.md`, `SUCCESS.md`, and `TESTING.md` reflect the final state.

## Suggested first Andi slice

The first implementation slice should be deliberately small:

1. Add structural OMT classifier.
2. Add lead id/upsert helpers.
3. Add tests for forest/town/open classification.
4. Add tests that harvested/dangerous existing leads suppress refresh.
5. Run Ralph Wiggum pre/post pass on just those seams.

Do not start with live harness. If the classifier/memory seam is wrong, the live probe will only produce expensive nonsense with better lighting.
