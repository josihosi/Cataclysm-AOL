# Bandit live signal + site bootstrap correction v0 (2026-04-26)

Status: active / partial implementation checkpoint.

## Normalized contract

**Title:** Bandit live signal + site bootstrap correction v0

**Request kind:** Josef correction / parked implementation package

**Summary:** Josef's 2026-04-26 live playtest exposed a real chain break, not a tuning complaint: the running world can have a bandit camp about 10 OMT away and a sustained visible fire, while `bandit_live_world` has no registered owned sites and the live dispatcher has no fire/light/smoke lead to consume. This packet corrects the live bootstrap, signal intake, range matrix, cadence, and observability story so the deterministic smoke/light/mark proof seams can become real gameplay pressure without pretending every bandit exists globally from world start.

## Evidence this package is based on

- Current save evidence from the playtest had `.userdata/dev-harness/save/McWilliams/dimension_data.gsav` with `"bandit_live_world": { "owner_id": "hells_raiders_live_owner_v0", "sites": [ ] }`.
- Current debug evidence had no `local_gate site=`, `live_dispatch_goal`, `dispatch ready`, or `shakedown_surface` lines, so the dispatcher was not merely choosing wrong; it had no owned live site to work from.
- Current code reads `src/do_turn.cpp::steer_live_bandit_dispatch_toward_player()` from `overmap_npc_move()` on `calendar::once_every( 30_minutes )` and filters candidate sites with hard `distance <= 10`.
- Current ownership is created by tracked NPC/mapgen spawn claims (`src/mapgen.cpp::map::place_npc()` -> `bandit_live_world::claim_tracked_spawn(...)`), so a camp can become owned when the player gets close enough for the roster to spawn/load, but a remote overmap special can remain abstract terrain with no live owner record.
- Current smoke/light adapters are real deterministic footing, not live intake by themselves: `adapt_smoke_packet()` caps meaningful smoke at 15 OMT, and `adapt_light_packet()` can project ordinary/searchlight/elevated light farther depending on source, persistence, concealment, weather, and elevation.

## Corrected range matrix

This matrix supersedes the earlier loose 48/60 OMT starter lean for the main system range. The older numbers were useful anti-tripwire pressure, but Josef's corrected product target for the overmap AI/system envelope is now **40 OMT**.

| Layer / signal | Starter value | Meaning |
| --- | ---: | --- |
| Map scale | `1 OMT = 24 map tiles` | Keep OMT counts grounded; one OMT is roughly house-scale, not county-scale. |
| Bubble-local high relevance | about `3 OMT` | Loaded/reality-bubble-adjacent logic. Do not confuse this with the strategic system. |
| Overall overmap AI / system envelope | `40 OMT` radius | Maximum region in which bandit overmap AI may maintain candidates, marks, and strategic pressure. This replaces the current hard `10` candidate gate and the older 48/60 starter lean. |
| Ordinary bounty visibility | about `10 OMT` | Clear-condition ordinary “something profitable may be there” visibility; useful as a signal family cap, not as the whole system range. |
| Confident threat visibility | about `6 OMT` | Sharper danger read should usually require closer or harder evidence. |
| Searchlight / hard-danger threat extension | about `8 OMT` | Stronger threat read with corroboration such as searchlights, patrols, prior losses, or explicit danger evidence. |
| Meaningful sustained smoke plume | about `15 OMT` cap | Smoke can project beyond ordinary sight when conditions support it; weak, brief, rainy, sheltered, or wind-smeared smoke should read shorter/fuzzier. |
| Ordinary exposed night light | adapter-derived, often around `8-11 OMT` in current tests | Occupancy/bounty signal when genuinely exposed; daylight and containment can suppress it. |
| Exceptional elevated/exposed light | adapter-derived, currently proof-seam examples up to `30 OMT` | A radio-tower-style fire or other flagship exposed-high-fire case may matter farther out, but this is still bounded by the `40 OMT` system envelope and must not become universal z-level magic. |
| Daily movement budget | `1-6 OMT/day` by outing type | Movement is elapsed-time-earned travel credit, not a fresh jump every scheduler wake. |
| Dispatch cadence | currently `30 minutes` | Acceptable as a dispatch throttle, but not the whole signal law. Observation/mark creation and decay need their own cadence/events. |

Core correction: `10 OMT` is a useful ordinary visibility number, but it is not the overall overmap AI range. The live dispatcher must not use `distance <= 10` as the master gate for all bandit sites and signals. Ach, there was the trap: the useful local number dressed up as the whole system and started giving orders.

## Scope

1. **Bootstrap abstract owned hostile sites without player proximity.**
   - Register bandit/hostile overmap specials into `bandit_live_world` as abstract owned site records early enough that a remote camp can think, remember, and dispatch.
   - Store a cheap abstract roster/headcount/profile seed for each owned site.
   - Materialize concrete NPC bodies lazily on dispatch or when the reality bubble reaches the site, then reconcile materialized members back into the same owned-site ledger.

2. **Wire real live signal intake into the existing mark/lead footing.**
   - Convert bounded live fire/smoke/light observations into the existing smoke/light mark-generation packets or equivalent source packets.
   - Respect current weather and light conditions when generating the live signal: daylight, darkness, fog/mist, rain, wind, shelter/containment, source strength, persistence, and exposure should affect confidence/range instead of being decorative test variables.
   - Reuse local reality-bubble truth; do not invent a full global smoke or light simulator.
   - Let bubble-local activity refresh abstract overmap marks that can persist beyond the loaded field itself.

3. **Replace the hard `<= 10` candidate gate with the range matrix.**
   - Candidate gathering should consider owned sites and relevant marks/leads inside the `40 OMT` system envelope.
   - Signal-specific families then apply their own caps and confidence decay: smoke around `15`, ordinary exposed light around current adapter values, hard/elevated cases bounded by adapter output and the `40 OMT` system envelope.
   - Distance should be a scoring burden after eligibility, not the first blunt axe that prevents signal-specific ranges from mattering.

4. **Separate signal observation/maintenance cadence from dispatch decisions.**
   - New signal events may create/refresh marks immediately.
   - Nearby/hot signal maintenance can use the existing 20-minute style family where appropriate; inactive/distant refresh can be coarser; daily cleanup remains for drift/collapse.
   - Dispatch may remain throttled around `30_minutes`, but that throttle must consume accumulated marks/leads rather than being the only moment the world notices signals.

5. **Make the failure modes reviewer-readable.**
   - Add logs/counters/debug reports for owned-site count, candidate scan radius, candidate distance, rejected-by-range, generated signal packets, signal-family cap, dispatch cadence skip, and empty ownership state.
   - A future Josef/Schani bug report should be able to distinguish “no owned site,” “signal never generated,” “signal generated but under threshold,” “candidate rejected by range,” “dispatch held by cadence,” and “dispatch chose hold/chill.”

6. **Prove the live chain, not just the proof seam.**
   - Keep deterministic tests for the matrix and packet math.
   - Add at least one harness/live proof where a real current fire/light/smoke source creates a live bandit mark/lead or dispatch candidate through the real game path.
   - Include a no-signal/no-smoke control for the reported fire/camp setup before claiming smoke/light caused the behavior.

## Non-goals

- No eager global spawning of every concrete bandit NPC from every overmap special.
- No full global smoke simulation or tile-perfect world-map light ray theatre.
- No broad faction-AI rewrite, diplomacy system, or settlement-signature mythology.
- No reopening the already-closed deterministic proof packets just to rename them.
- No claim that deterministic adapter/playback tests are live gameplay proof until the live source hook exists.
- No visible-light-to-zombie-horde bridge in this packet unless it is deliberately pulled in; that bridge is already parked separately in `doc/bandit-live-wiring-audit-and-light-horde-bridge-correction-v0-2026-04-26.md`.
- No Basecamp medical-consumable work here; keep that as `Basecamp medical consumable readiness v0` unless Josef explicitly bundles it.

## Success state

- [ ] Existing hostile overmap special families that should participate in live hostile-site logic can register abstract `bandit_live_world` site records without requiring the player to enter spawn/load range first.
- [ ] Abstract site records carry enough cheap roster/profile/headcount state to dispatch and later materialize concrete NPCs without save/perf blow-up.
- [ ] Materialized NPCs reconcile back to the same owned-site ledger, preserving existing exact-member writeback behavior when concrete members exist.
- [ ] Real fire/smoke/light observations can create or refresh bounded live bandit marks/leads through the running game path, not only authored playback packets, and the live generation respects weather/light conditions such as daylight, darkness, fog/mist, rain, wind, shelter/containment, source strength, persistence, and exposure.
- [ ] The overall overmap AI/system envelope is `40 OMT`, while smoke, ordinary light, threat, and exceptional elevated light keep their signal-specific caps inside that envelope.
- [ ] The hard `distance <= 10` live-dispatch gate is removed or demoted so `10 OMT` ordinary visibility no longer impersonates the whole system range.
- [ ] Signal observation/decay cadence is separate from dispatch decision cadence, with event-driven creation and reviewer-readable maintenance.
- [ ] Instrumentation distinguishes empty ownership, no signal packet, below-threshold signal, rejected-by-range, cadence skip, and hold/chill decisions.
- [ ] Deterministic tests cover the range matrix, site bootstrap serialization, signal-specific caps, and candidate filtering/scoring split.
- [x] At least one harness/live proof shows a real current fire/light/smoke source producing a live bandit candidate or mark on a real owned-site path, plus one no-signal control for the same setup.

## Current implementation notes

Source checkpoint in progress:

- Abstract ownership slice: `overmap_npc_move()` bootstraps hostile overmap-special site records from already-loaded overmaps before player-proximity NPC materialization, using the same owned-site ledger that later concrete spawn claims reconcile into.
- Range slice: live dispatch now scans the `40 OMT` system envelope and treats `10 OMT` as only the direct-player cap, not the master candidate gate.
- Minimal signal slice: loaded-map `fd_fire` / `fd_smoke` fields near the player now build live smoke packets with current weather, pass through `adapt_smoke_packet()`, and can make owned sites inside the smoke projection cap into `live_signal` dispatch candidates.
- Signal memory slice: accepted live signal candidates refresh the owned site's remembered mark / `known_recent_marks` via a bounded ledger helper instead of staying as throwaway local variables.
- Lazy materialization slice: selected abstract overmap-special candidates can create only the minimum profile-specific concrete roster needed to attempt scout dispatch, then reconcile those NPCs through `claim_tracked_spawn()` into the same owned-site ledger before planning; failed claims are logged and skipped before overmap insertion.
- Observability slice: logs now identify no signal packet, below-threshold smoke projection, signal packet id, weather band, candidate distance, cap used, rejected-by-range count, cadence skip, missing concrete member, route failure, lazy materialization counts, and failed lazy materialization claims. Decay and full hold/chill signal explanations remain open.
- Harness proof slice: `bandit.live_world_nearby_camp_smoke_mcw` injects real saved `fd_fire` / `fd_smoke` fields into a copied McWilliams harness save using v39 field source objects and safe in-bubble/off-player offsets, opens the in-game wait command with `|`, chooses `w`ait a while from the alarm prompt, and selects `7` for a three-hour wait. The current positive run `.userdata/dev-harness/harness_runs/20260427_000610/` records initial `signal_packet=yes`, matched/refreshed signal maintenance, and 5-minute signal / 30-minute dispatch cadence-skip evidence, then records the expected later `signal_packet=no` / `signal_packet=none` shape as the seeded short-lived fields decay during the wait. The paired control `.userdata/dev-harness/harness_runs/20260427_001649/` uses the same nearby camp footing and same wait path without injected fields and records only `signal_packet=no`. Earlier debug-screen/on-fire scratch runs are not product proof.

Still not closed:

- live light intake and shelter/exposure/detail beyond the smoke adapter's current packet fields, unless this packet is narrowed to smoke/fire and leaves live light to the next greenlit bridge;
- separated signal decay cadence;
- reviewer-readable hold/chill evidence.

## Suggested validation packet

Minimum implementation validation:

1. Narrow deterministic tests:
   - site bootstrap/save-load for abstract owned sites;
   - lazy materialization/reconciliation if concrete bodies are created;
   - `40 OMT` master envelope with `10/6/8/15/30` signal-family caps preserved as separate concepts;
   - dispatch candidate generation proving a `15 OMT` smoke lead is not rejected by the old `<= 10` gate.
2. Source gates:
   - touched-object compile for `src/bandit_live_world.cpp`, `src/do_turn.cpp`, and any signal-hook source;
   - focused `./tests/cata_test "[bandit][live_world]"` and relevant mark-generation/playback filters.
3. Live/harness proof:
   - one owned hostile site that was not made live solely by walking the player onto the camp;
   - one real fire/light/smoke source that generates a mark/candidate through live hooks;
   - one otherwise identical no-signal control;
   - saved-world or artifact log showing site count, signal packet, candidate distance, cap used, and dispatch/hold decision.

## Handoff note for Andi

Start with the live chain in this order:

1. ownership/site bootstrap,
2. signal packet creation from real map footing,
3. range/cadence correction,
4. instrumentation,
5. proof.

Do not start by spawning every bandit in the county. That would be impressive in the same way flooding the kitchen proves the sink works.