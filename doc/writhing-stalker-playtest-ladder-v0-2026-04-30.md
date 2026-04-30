# Writhing stalker playtest ladder v0

Parent packet: `doc/writhing-stalker-behavior-packet-v0-2026-04-30.md`

Imagination source: `doc/writhing-stalker-imagination-source-of-truth-2026-04-30.md`

## Testing principle

Do not prove horror with a spreadsheet and then call the monster alive. Also do not prove horror with vibes and then pretend the code has manners.

The stalker needs a ladder:

1. **Creature truth** — what exists in JSON/spawn data?
2. **Rarity truth** — can it avoid becoming ordinary monster soup?
3. **Interest truth** — what signals attract it?
4. **Latch truth** — when does it fixate, and when does it stop?
5. **Approach truth** — does it prefer cover/darkness/clutter instead of beelining?
6. **Opportunity truth** — does it wait for a vulnerable/distracted player?
7. **Strike truth** — does it hit fast with cut/bleed pressure?
8. **Withdrawal truth** — does it leave when exposed/hurt/focused?
9. **Persistence truth** — does any new state survive save/load honestly?
10. **Live truth** — does a real scene feel like a stalker, not a test puppet?

## Rung 1 — creature definition

Evidence class: JSON/schema plus narrow deterministic checks where available.

Checks:

- monster id, name, description, species/material/body shape are correct;
- HP/speed/dodge/armor fit the first-pass target band;
- attacks create cut/bleed threat without brute-wall damage;
- flags support agility, fear, retreat, and singleton feel;
- spawn groups are rare and controlled.

Smell test:

- If it is just a faster zombie with a purple prose description, red.
- If it is more elite than predator/prowler by accident, red or tuning-yellow.

## Rung 2 — rarity / singleton guard

Evidence class: deterministic spawn/JSON inspection plus optional live sanity.

Checks:

- ordinary worldgen does not flood stalkers;
- debug/harness can spawn one intentionally;
- natural spawning, if enabled, is rare and biome/context bounded;
- no group spawn unless explicitly designed later.

Live probe ideas:

- `writhing_stalker.spawn_singleton_debug_mcw`
- `writhing_stalker.spawn_rarity_json_audit`

Green means one stalker can be summoned for proof and ordinary spawn config is not spammy. It does not require broad natural-world statistical proof in v0.

## Rung 3 — interest source tests

Evidence class: deterministic helper/evaluator tests.

Scenarios:

- `stalker_interest_prefers_recent_human_mark_over_empty_terrain`
- `stalker_interest_exposed_night_light_is_strong_lure`
- `stalker_interest_smoke_is_weak_indirect_signal`
- `stalker_interest_forest_and_building_edges_are_cover_value`
- `stalker_interest_zombie_pressure_increases_opportunity_not_threat`

Required assertion shape:

```text
interest_source=<human|light|terrain|zombie_pressure|smoke>
score=<n>
confidence=<n>
reason=<short text>
```

Smell test:

- If smoke acts like food, wrong creature.
- If zombie density scares it like a bandit camp, wrong interpretation.

## Rung 4 — latch and leash tests

Evidence class: deterministic contract plus save/load if persisted.

Scenarios:

- `stalker_latches_recent_player_evidence_with_timeout`
- `stalker_latch_does_not_become_permanent_tracking`
- `stalker_latch_refresh_requires_new_evidence`
- `stalker_latch_breaks_after_distance_time_or_exposure`
- `stalker_ignores_stale_far_player_mark`

Required states:

```text
unlatched -> interested -> latched -> shadowing -> cooling_off
```

Smell test:

- If it always knows where the player is after one clue, red.
- If it forgets instantly and never creates dread, yellow/red depending on intent.

## Rung 5 — approach / no-beeline tests

Evidence class: deterministic path/choice test, then live screenshot/log proof if connected.

Scenarios:

- `stalker_approach_prefers_cover_over_open_beeline`
- `stalker_approach_holds_at_bright_exposure`
- `stalker_approach_uses_forest_or_building_edge`
- `stalker_approach_does_not_teleport_to_target`

Metrics:

- chosen approach class;
- whether direct line-of-sight is avoided where alternatives exist;
- distance closed per tick/window;
- exposure reason if it holds.

Smell test:

- If the creature just pathfinds straight across a football field, it is not stalking. It is commuting.

## Rung 6 — opportunity window tests

Evidence class: deterministic decision tests.

Scenarios:

- `stalker_waits_when_player_is_alert_and_unpressured`
- `stalker_strikes_when_player_is_bleeding_low_stamina_and_distracted`
- `stalker_strikes_when_zombies_create_opening`
- `stalker_holds_when_player_has_space_light_and_focus`
- `stalker_opportunity_noise_can_raise_pressure_without_forcing_attack`

Required report:

```text
latch=yes/no opportunity=<n> vulnerability=<n> zombie_distraction=<n> exposure=<n> decision=<hold|shadow|strike|withdraw>
```

Smell test:

- If every latch becomes immediate attack, the horror has no timing.
- If it never attacks, congratulations, we implemented anxiety with legs.

## Rung 7 — strike / damage feel tests

Evidence class: deterministic combat/effect checks plus live playtest.

Scenarios:

- `stalker_strike_applies_cut_bleed_pressure`
- `stalker_strike_is_short_not_boxing_match`
- `stalker_damage_can_be_survived_by_prepared_player`
- `stalker_damage_punishes_bad_state_without_instant_unfair_death`

Live packet should record:

- player condition before strike;
- stalker condition before strike;
- hit/effect lines;
- wounds/bleed outcome;
- how quickly the stalker withdraws or continues.

Smell test:

- If the player dies because the numbers are huge, not good horror.
- If the player ignores it like a mosquito, also not good horror. A very stylish mosquito, but still.

## Rung 8 — withdrawal / cooldown tests

Evidence class: deterministic contract plus live proof if possible.

Scenarios:

- `stalker_withdraws_when_hurt`
- `stalker_withdraws_when_exposed_and_focused`
- `stalker_cooldown_blocks_immediate_repeat_attack`
- `stalker_can_reengage_only_after_new_evidence`

Required state:

- withdrawal reason is named;
- cooldown is visible;
- repeated attacks without refreshed evidence are blocked.

## Rung 9 — persistence tests

Evidence class: save/load serialization proof.

Only required if new latch/cooldown state is stored outside existing monster state.

Scenarios:

- save while latched/shadowing;
- reload and verify latch age/target confidence did not reset to omniscience;
- save during cooldown;
- reload and verify no immediate repeat attack.

If v0 avoids new persistent state, record that explicitly and do not invent fake serialization work.

## Rung 10 — live/harness playtest packet

Evidence class: feature-path live/harness proof.

### Green live packet A — stalk and strike

Proposed scenario: `writhing_stalker.live_shadow_strike_mcw`

Setup:

- one debug-spawned stalker at plausible distance;
- player in town/forest/building-edge route;
- valid human/light/noise clue;
- zombie pressure or vulnerability setup;
- no teleport/spawn-on-top trick.

Proof steps:

1. Startup/load green.
2. Stalker exists, singleton confirmed.
3. Player produces or already has a valid clue.
4. Stalker latch/interested state is recorded.
5. Wait/move window proves approach/shadowing rather than instant beeline.
6. Opportunity window is created.
7. Strike occurs with cut/bleed pressure.
8. Stalker withdraws or changes state for a named reason.
9. Save/audit records no immediate repeat spam.

### Green live packet B — no-evidence negative

Proposed scenario: `writhing_stalker.live_no_omniscient_beeline_mcw`

Setup:

- stalker exists somewhere plausible;
- player gives no valid clue;
- no night light lure or human mark;
- wait window long enough to catch cheap omniscience bugs.

Green means no instant beeline/attack and no magic refreshed latch.

### Green live packet C — exposed-focus retreat

Scenario: `writhing_stalker.live_exposed_retreat_mcw`

Status: **GREEN** at `.userdata/dev-harness/harness_runs/20260430_163626/`.

Credited setup/evidence:

- harness-only noon fixture `mcwilliams_live_debug_noon_2026-04-30` applies a saved `game_turn` transform only; no artificial map/light/field setup is synthesized;
- pre-spawn saved-turn audit proves `time_of_day_text=12:00:00` with zero noon delta;
- one hostile `mon_writhing_stalker` is spawned at plausible visible distance;
- same-run live-plan artifact proves `decision=withdraw route=hold_exposed reason=live_exposed_and_focused_withdraw ... stalker_bright=yes target_focus=yes cooldown=no`;
- save/writeback mtime and saved active-monster audit remain green after the live line.

Green means stalker holds/withdraws instead of brainless melee until death.

### Green live packet D — mixed hostile performance metrics

Proposed scenario: `performance.mixed_hostile_stalker_horde_mcw`

Contract: `doc/mixed-hostile-stalker-horde-performance-playtest-v0-2026-04-30.md`

Setup:

- one bandit camp / hostile live-world site;
- one cannibal camp / hostile live-world site;
- exactly one `mon_writhing_stalker` at a plausible distance;
- one overmap horde or horde-pressure fixture;
- a bounded player position/state that lets the run measure mixed pressure before it collapses into nonsense.

Required report:

- runtime commit and dirty/clean state;
- scenario/run path;
- setup mix and proof all four requested actors exist;
- in-game window, sampled turns and/or waited minutes;
- harness wall-clock;
- per-turn min/median/p95/max where available;
- `bandit_live_world perf` min/median/max plus max `signal_us`, `dispatch_us`, `travel_us`, and active job mix;
- stalker planning cost and horde movement/retarget cost, or explicit `not instrumented` for either;
- debug/log spam, stability, and playability note.

Green means the mixed scene completes with real metrics from live paths. Startup/load-only, setup-only, or missing-ingredient runs are not green.

### Optional Josef playtest prompts

Josef should not be a blocker, but if he plays it, ask only product questions:

- Did the first glimpse land?
- Did the attack feel earned or cheap?
- Was the bleed scary or just annoying?
- Did the retreat feel clever, cowardly, or broken?
- Did rarity make it special or invisible?

## Closure gates

- [ ] Creature JSON/schema green.
- [ ] Rarity/singleton proof green.
- [ ] Interest/latch deterministic tests green.
- [ ] Approach/no-beeline deterministic proof green.
- [ ] Opportunity/strike deterministic proof green.
- [ ] Withdrawal/cooldown deterministic proof green.
- [ ] Persistence proof green or explicitly not applicable.
- [ ] Live packet A stalk/strike green.
- [ ] Live packet B no-omniscient-beeline green.
- [ ] Live packet C exposed/focus retreat green or explicitly future-only.
- [ ] Mixed hostile performance packet `performance.mixed_hostile_stalker_horde_mcw` records bandit camp + cannibal camp + stalker + horde metrics, or is explicitly classified as follow-up/future-only.
- [ ] Tuning readout records too-fast/too-tanky/too-common/too-stupid/too-expensive verdict.

## Future-only after v0

- shared overmap-interest substrate for multiple hostile types;
- recurring named individual/nemesis memory;
- richer wound infection/psychological pressure;
- multi-night stalking arcs;
- interactions with weather/scent/noise at higher fidelity;
- special autopsy/lore reveals.
