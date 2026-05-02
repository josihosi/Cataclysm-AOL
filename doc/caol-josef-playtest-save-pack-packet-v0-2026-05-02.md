# CAOL-JOSEF-PLAYTEST-SAVE-PACK-v0 packet (2026-05-02)

Status: ACTIVE / GREENLIT / JOSEF PLAYTEST SAVE-PACK PREP

Owner: Andi for save/handoff preparation and verification; Schani for final Josef-facing framing.

Imagination source: `doc/caol-josef-playtest-save-pack-imagination-source-2026-05-02.md`.

Supersedes/promotes from: `CAOL-VISIONS-PLAYTEST-SAMPLER-v0` relay/card-only state.

## Intent

Prepare a labelled pack of current-build saves or handoff sessions that Josef can load and playtest across the current C-AOL feature mess: Basecamp AI/camp locker, bandit pressure, cannibal camp, flesh raptor, zombie rider, and writhing stalker.

This is product-taste preparation, not another closed-lane proof ritual. Existing green rows are allowed as footing, but the deliverable is a usable playtest pack with clear labels, load checks, caveats, and short instructions.

## Scope v0

Preserve the existing playtest family labels while turning them into loadable entries: `/visions/camp-locker`, `/rider/stalker/raptor`, and `/horde/camp`.

Prepare one usable entry each for:

1. **Basecamp AI / camp locker usefulness**
   - Include camp board/assignment/usefulness where feasible plus camp locker / zone stock service.
   - Existing footing may include `locker.zone_manager_save_probe_mcw`, `locker.weather_wait`, basecamp follower/board probes, and the current camp locker proof docs.

2. **Bandit pressure / shakedown / basecamp contact**
   - Prefer an entry that reaches visible pressure quickly enough for a human playtest.
   - Existing footing may include scenic shakedown, extortion at camp, first-demand fight/pay, reopened demand, or prepared-base contact rows.

3. **Cannibal camp pressure**
   - Use the best current cannibal camp/local-contact/smoke pressure footing.
   - Existing footing may include `cannibal.live_world_night_local_contact_pack_mcw`, `cannibal.live_world_day_smoke_pressure_mcw`, and related cannibal camp rows.

4. **Flesh raptor**
   - Prepare a skirmisher/circling or equipment-frustration comparison entry.
   - Existing footing may include `flesh_raptor.live_open_field_skirmisher_mcw`, `flesh_raptor.live_crowded_arc_skirmisher_mcw`, and `flesh_raptor.live_equipment_frustration_comparison_mcw`.

5. **Zombie rider**
   - Prepare a late-world predator/counterplay entry.
   - Existing footing may include `zombie_rider.live_open_field_pressure_mcw`, `zombie_rider.live_cover_escape_mcw`, `zombie_rider.live_camp_light_band_mcw`, and close-pressure proof run `.userdata/dev-harness/harness_runs/20260502_050055/`.

6. **Writhing stalker**
   - Prepare the current hit-fade retreat rhythm and/or zombie-shadow pressure entry.
   - Existing footing should include the fresh hit-fade proof `writhing_stalker.live_hit_fade_retreat_mcw` -> `.userdata/dev-harness/harness_runs/20260502_113738/`. Avoid old dirty quiet-side optical footing unless rerun clean.

## Camp/NPC assignment preflight

Before any camp-threat or bandit contrast row is credited, audit the current harness save's NPCs for real camp membership/assignment. Unassigned NPCs are noise, not low-threat or high-threat proof. For low-threat rows, remove/kill/despawn or otherwise neutralize those extras so the scene is genuinely quiet. For high-threat rows, spawn or repair NPCs as properly camp-assigned members tied to the intended camp/faction/overmap-special state, then record the audit path and caveat.

## Thematic contrast pass

Before Josef's playtest card is called ready, try a small contrast matrix for the two most state-sensitive families:

- **Writhing stalker:** no-fire / low-threat setup should let it close, whack, then fade out; fire/light setup should make it keep stalking or hesitate instead of committing stupidly; high-threat player/support setup should bias toward stalking/pressure rather than a suicidal rush.
- **Bandits:** no obvious camp signal should avoid instant theatrical pressure; fire/smoke/basecamp signal should pull visible scouting, shakedown, or standoff pressure; high-threat/resistant player/camp setup should bias toward stalking, holding off, or backing out instead of repeating the same demand like a tax office with crossbows.

This matrix is product-taste evidence. It can be delivered as paired save entries, paired handoff reports, or a caveated card if one contrast is blocked, but it must not silently collapse into one generic “bandit/stalker works” row.

## Deliverable shape

Create a Josef-facing save-pack card under `doc/` and, where useful, a prepared save/handoff artifact directory. Each entry must include:

- label;
- save path, handoff command, or exact launch/load path;
- 2-5 minute “what to try” instructions;
- expected visible/mechanical read;
- 2-4 taste questions;
- current-build load/startup check result;
- artifact directory or proof footing;
- caveats: staged vs natural, known dirty rows, blocked/no-credit rows, portal-storm warning status.

## Evidence / validation expectations

Before claiming the save pack is ready:

- run `git diff --check` for docs/harness edits;
- for any code or harness changes, run the narrow relevant compile/test gate;
- for each prepared playable entry, run a current-build `probe` or `handoff`/startup check sufficient to prove it loads and reaches the intended start state;
- inspect `probe.report.json`, `handoff.report.json`, step ledger, portal-storm warning, and relevant runtime warnings;
- record any unplayable/blocked entry plainly instead of calling it ready;
- if a fresh GUI/handoff is created, record cleanup/handoff status and PID/session path where applicable.

## Success state

- [ ] Six labelled playtest entries exist: Basecamp AI/camp locker, bandit pressure, cannibal camp, flesh raptor, zombie rider, writhing stalker.
- [ ] Each entry has a loadable save/handoff/setup path or is explicitly marked blocked with the missing primitive.
- [ ] Each ready entry has current-build load/start-state evidence and no silent portal-storm contamination.
- [ ] Each entry has short Josef-facing instructions and focused taste questions.
- [ ] Existing proof rows are cited only as footing and staged-vs-natural caveats stay visible.
- [ ] The final Josef-facing card is short enough to use without log archaeology.
- [ ] Any blockers become concrete follow-up packets or caveated omissions, not hidden failures.

## Non-goals

- No release packaging.
- No broad new implementation unless required to make a save loadable/playable.
- No reopening closed v0 lanes by drift.
- No balancing pass or tuning spree hidden inside save prep.
- No asking Josef to inspect logs as primary validation.

## First Andi move

Inventory the best existing scenario/handoff footing for the six entries, then prepare the smallest usable current-build save/handoff pack. Start from existing green rows and only rerun or create fresh handoffs where the current entry would otherwise be stale, dirty, blocked, or not actually playable.