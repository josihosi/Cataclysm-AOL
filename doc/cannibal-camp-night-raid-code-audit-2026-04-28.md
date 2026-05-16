# Cannibal camp night-raid code audit (2026-04-28)

Status: SCHANI AUDIT / DETERMINISTIC SUBSTRATE LANDED / LIVE SLICE PARTIAL GREEN

Imagination source: `doc/cannibal-camp-night-raid-imagination-source-of-truth-2026-04-28.md`

Audited against the intended scene: cannibal camps remain camp-shaped hostile sites, but they are more vicious than bandits, do not extort, should prefer pack attacks over lone suicide attackers, and should pounce at night/darkness or similarly concealed killing windows.

## Current checkpoint after first implementation packet

The first deterministic/code packet has landed. It adds profile-specific cannibal pack sizing, a smoke/light nearby-lead distinction between scout/probe pressure and pack attack pressure, `local_gate_input::darkness_or_concealment`, live `current_exposure` feeding into the local gate, current/recent exposure hold-off, bounded sight-avoid reposition helper proof, high-threat abort preservation, no-extort/no-shakedown reporting, and multi-member active-group save/load proof.

Live product proof is now partial green under the proof-freeze rules: day smoke/light pressure (`20260428_124902`) reaches cannibal stalk/hold-off rather than instant combat; night local contact (`20260428_124947`) reaches `posture=attack_now` with `pack_size=2`, `darkness_or_concealment=yes`, and `shakedown=no`; exposed/recent-sight footing (`20260428_125138`) holds off instead of crediting a visible beeline; day-smoke repeatability passes (`20260428_125319`, `20260428_125342`). Saved-state persistence is green for a real active cannibal group/profile/target/live-signal mark (`20260428_130948`); the no-fixture reload audit (`20260428_131031`) is clean reload/saved-file support only because it reads saved `dimension_data.gsav` and reports `inconclusive_no_new_artifacts`. `intelligence_map.leads=[]` remains out of scope; the remaining live gap is only an explicit high-threat/comparable daylight negative if Schani wants it separated. Treat the detailed gap analysis below as the original pre-implementation map plus the remaining optional negative/control proof burden.


## Original implemented footing before this packet

The current code already has real cannibal-camp substrate:

- `owned_site_kind::cannibal_camp` and `hostile_site_profile::cannibal_camp` exist in `src/bandit_live_world.h` / `src/bandit_live_world.cpp`.
- `rules_for_profile( hostile_site_profile::cannibal_camp )` gives cannibal camps distinct profile rules:
  - `home_reserve = 2`
  - `scout_job_bonus = 2`
  - `threat_penalty = 0`
  - `retreat_bias_floor = 3`
  - `return_clock_floor = 3`
  - `default_remaining_pressure = tight`
  - report text: `hungry camp pressure`.
- `claim_tracked_spawn()` can claim `cannibal_camp` spawns/sites and assigns the cannibal profile.
- `plan_site_dispatch()` and `apply_dispatch_plan()` carry profile, selected members, active group, active target, and save/load state through the same hostile-site machinery as bandits.
- `choose_local_gate_posture()` already diverges cannibal behavior from bandits at the encounter surface:
  - favorable contact plus pressure can become `attack_now`;
  - cannibals do not open `open_shakedown`;
  - weaker opportunity can become `probe` or `stalk`;
  - overwhelming local threat can still abort/hold off.
- `build_shakedown_surface()` explicitly blocks `cannibal_camp` even if a local gate ever tried to open shakedown.
- Deterministic tests prove current profile separation:
  - `bandit_live_world_keeps_cannibal_camp_separate_from_bandit_camp_ownership`
  - `bandit_live_world_makes_cannibal_camp_attack_instead_of_extort`
- Existing canonical docs close the older limited packets:
  - `doc/cannibal-camp-first-hostile-profile-adopter-packet-v0-2026-04-22.md`
  - `doc/cannibal-camp-attack-not-extort-correction-v0-2026-04-24.md`

Verdict on footing: the drawer exists, the label is not fake, and the no-extort behavior is real deterministic substrate.

## Original gaps against the imagination source

### 1. Single-cannibal attack is still allowed by default

Current tests explicitly expect a cannibal dispatch with one active member:

- `tests/bandit_live_world_test.cpp` checks `cannibal_camp.dispatchable_member_capacity() == 1` in the profile separation test.
- The same test applies a valid cannibal dispatch and expects `active_member_ids == { character_id( 760 ) }` after save/load.

Current dispatch member count comes from the generic job path:

- `plan_site_dispatch()` gets `required_members = required_dispatch_members( winner.job )`.
- `select_dispatch_members()` then takes exactly that count from at-home members.
- The nearby target lead still says `bounded v0 dispatch only promotes scout pursuit from real owned members`, so the current path is still scout/small-handoff biased.

This contradicts the new vision for planned attacks: cannibals should not normally send one doomed attacker as the whole raid.

### 2. Smoke/light scouting chain is not yet cannibal-specific

The shared live-signal/camp-map direction already has smoke/light/human opportunity footing elsewhere in the bandit work, but this cannibal packet has not yet proven a cannibal-specific chain: smoke/light or repeated human routine should become a lead, the lead should justify scouting/stalking/dispatch pressure, and only a later favorable night/darkness/concealment window should justify the pack attack. Raw smoke/light must not become instant cannibal combat.

### 3. Night/darkness is not represented in local-gate input or dispatch planning

`local_gate_input` currently has:

- `local_threat`
- `local_opportunity`
- `standoff_distance`
- `basecamp_or_camp_scene`
- `rolling_travel_scene`
- `recent_exposure`
- `local_contact_established`

It has no field for:

- night/darkness;
- light exposure;
- weather/visibility;
- cover/concealment;
- explicit sight/exposure state for stalking cannibals;
- target sleeping/resting routine;
- time-window preference.

`plan_site_dispatch()` also has no time-of-day or darkness argument, and no profile-specific night raid sizing. Therefore darkness cannot currently make cannibals more likely to attack except as prose in a future note.

### 4. Stalking sight-avoidance is not yet part of the cannibal promise

Bandit sight-avoidance/standoff footing exists elsewhere in the live-world work, but this cannibal packet has not yet required or proven that cannibal stalking uses it. The new product requirement is explicit: stalking cannibals should avoid player/basecamp vision, and exposure should produce bounded reposition, hold-off, or abort rather than continued visible approach.

### 5. Cannibal trigger-happiness exists only after generic pressure math

Current cannibal behavior in `choose_local_gate_posture()` is real, but it is downstream of generic pressure math:

- `pressure_margin = active_member_ids.size() + local_opportunity - local_threat`.
- cannibals attack when `local_contact_established && pressure_margin >= 2`.
- cannibals probe when `local_opportunity > 0 && pressure_margin >= 0`.
- otherwise they stalk.

That gives the profile a lethal/no-extort surface, but not yet a distinctive “wait until dark, then pounce with a pack” behavior.

### 6. Existing cannibal docs closed a smaller promise

The closed docs promised:

- a real second hostile-site profile;
- rare dedicated cannibal camp anchor/theme;
- no bandit shakedown/extortion surface;
- deterministic local-gate/profile split.

They did **not** promise:

- larger raid groups;
- smoke/light lead -> scout/stalk/dispatch -> night attack chain;
- night preference;
- darkness as a combat/opportunity modifier;
- pack threshold versus lone attacks;
- live/harness proof for night ambush behavior.

So the new lane should not reopen those closed packets as failures. It should build on them.

## Recommended bounded implementation slices

### Slice 1 — Cannibal pack-size dispatch rule

Goal: prevent ordinary cannibal planned attacks from being one-member suicide attacks.

Likely shape:

- Add profile-specific desired/required member count logic above or beside `required_dispatch_members()`.
- For `cannibal_camp`, distinguish scout/stalk/probe from attack/raid pressure:
  - scout/probe may still be one member when the job is clearly reconnaissance;
  - attack/raid pressure should require a minimum pack, probably 2-3 depending on available roster;
  - if not enough at-home members remain after reserve, block attack and choose scout/stalk/hold instead.
- Keep existing home reserve / dead-wounded-away capacity honest.

Deterministic proof:

- 1 dispatchable cannibal: no planned attack; scout/stalk/probe/blocked note instead.
- 2-3 dispatchable cannibals: valid attack/raid group uses multiple `active_member_ids`.
- Wounded/killed/outbound members reduce capacity and prevent magical pack attacks.
- Bandit dispatch member sizing stays unchanged.

### Slice 2 — Darkness/night opportunity input

Goal: give local gate and later dispatch planning a real variable that can change cannibal decisions.

Likely shape:

- Add a bounded field to `local_gate_input`, such as `darkness_advantage`, `night_or_dark`, or `concealed_approach`.
- Cannibal profile should use it to lower the attack threshold or raise `local_opportunity` only for cannibal attack logic.
- Daylight/no-cover should preserve stalk/probe/hold-off outcomes unless target weakness is overwhelming.
- Do not overfit exact astronomical time inside the pure evaluator unless the live caller can provide it cleanly. The first slice can be an explicit input seam plus tests; later live wiring can feed real time/light/visibility.

Deterministic proof:

- Same threat/opportunity/strength in daylight -> `probe` or `stalk`.
- Same inputs with darkness advantage -> `attack_now` for cannibals.
- Same darkness input does not make bandits open an attack-only cannibal path.
- High threat still aborts/holds even at night.

### Slice 3 — Live/cadence wiring for night pounce

Goal: connect the pure inputs to the actual game path.

Likely shape:

- Feed time-of-day/light/visibility or a coarse dark-approach bucket into the local gate from the real encounter path.
- Make reports name the input and outcome: `profile=cannibal_camp`, `darkness_advantage=yes`, `pack_size=N`, `posture=attack_now/probe/stalk`.
- Ensure active outing/group state carries enough information to avoid a scout becoming a full raid without a re-plan/writeback step.

Proof expectation:

- deterministic tests first;
- then a named harness/product scenario only after the harness proof-freeze rules can support it:
  - set up a cannibal camp with enough living members;
  - stage equivalent day and night/darkness target opportunities;
  - prove day outcome is not attack or uses hold/probe/stalk;
  - prove night outcome selects a multi-member attack group and reaches local attack posture;
  - prove no shakedown surface opens;
  - preserve save/load roster/active group state.

## Original recommended plan classification

This should be a new serious plan item, not an edit to the closed cannibal adopter/no-extort packets.

Suggested item:

`Cannibal camp night-raid behavior packet v0`

Current status:

- `ACTIVE / DETERMINISTIC SUBSTRATE LANDED`, because the first deterministic/code packet is now implemented and the harness trust-audit/proof-freeze lane is checkpointed/held rather than active.
- The bandit camp-map queue remains greenlit behind the current active priority unless Schani/Josef promote it separately.

Promoted first Andi implementation target:

`Cannibal camp pack-size + darkness/sight-avoid local-gate substrate v0`

Do not ask Andi to “make cannibals cooler.” Ask for exact seams:

1. no ordinary one-member cannibal attack/raid group;
2. smoke/light or repeated human routine becomes scout/stalk/dispatch pressure rather than instant combat where that substrate is available;
3. explicit darkness/concealment input in local gate;
4. explicit stalking sight/exposure response: seen cannibals reposition, hold off, or abort instead of visible beeline pressure;
5. deterministic comparison of daylight vs darkness outcomes;
6. bandit no-regression for shakedown/profile behavior;
7. reports naming lead source, profile, pack size, darkness input, sight/exposure state, and posture.

## Audit verdict

Foundation green; deterministic night-raid substrate now landed.

The pre-implementation audit found four missing seams: lone cannibal dispatch was still allowed, smoke/light leads did not have a cannibal-specific scout/stalk/dispatch chain, darkness/night was not represented in local-gate input, and stalking exposure behavior was not part of the cannibal promise. The first implementation packet now adds deterministic/code support for pack-size dispatch, smoke/light nearby-lead classification, explicit darkness/concealment local-gate input, live current-exposure feeding into that gate, current/recent exposure hold-off, bounded sight-avoid reposition helper proof, no-extort reporting, high-threat abort, and multi-member active-group save/load proof.

Remaining honest gap: this is still deterministic substrate, not live night-raid product proof. A later promoted slice must wire real time/light/visibility into the live path and prove the named behavior through actual dispatch/local-contact under proof-freeze rules.
