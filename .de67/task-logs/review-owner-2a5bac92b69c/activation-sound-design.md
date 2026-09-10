# R029-F004 activation and sound design

## Scope

This is a source-grounded implementation contract for the authorized repair. It does not change source, build, runtime state, saved games, or gameplay. The two defects have separate ownership boundaries and should remain separate in code and tests.

## Activation contract

The smallest activation change belongs in `src/do_turn.cpp`, in `materialize_committed_bandit_shakedown` (the analogous cannibal raid path is outside this repair). The existing sequence is the correct shape:

1. Validate the exact hostile reservation, `committed_contact` phase, `simulation_owner::local`, `local_contact` member states, alive exact `shared_ptr_fast<npc>` values from `overmap_buffer`, and empty placements.
2. Place every member first.
3. Call `g->load_npcs()` once. This preserves `game::load_npcs()` as the canonical tracker insertion and `on_load` owner; do not hand-register, copy, or call `reload_npcs()`.
4. Re-resolve each reservation ID and require the same shared pointer identity as the pre-placement vector. Require alive, `is_active()`, reality-bubble/in-bounds final position, and exactly one active tracker entry for every member. Require the reservation still contains each ID as `local_contact`.
5. On any failed postcondition, return a named admission rejection and stop before parley, attack, stalking, or outcome credit. Do not mutate phase, member state, demand, cursor, payment, return state, or strategic orders during retry.

The downstream local-contact and sight-avoid gates in `src/do_turn.cpp` must use the same active predicate. `g->find_npc()` alone is insufficient because `game::find_npc` can return an inactive overmap record. The exact pair must therefore be rejected as a local interaction opportunity until both records are active/tracker-present. Preserve the existing local stalking/hold-off exception and the exact rolling ambush/parley exclusion. Preserve the single movement owner: committed contact remains local tactical ownership, and Pay continues through the existing return handoff to `returning_home`/abstract routing.

The generic overmap loop already has several reservation exclusions. Add the committed-hostile exact-ID guard only if inspection during implementation shows a stale strategic route can still be consumed; otherwise do not broaden movement changes. The mechanically necessary activation repair is `load_npcs` plus postcondition validation.

## Sound field contract

Use the existing `camp_map_lead` fields; no new duration or invented timer is needed.

* `last_seen_minutes` is the sound event's `emitted_minutes`, and remains the source of the sound TTL. The existing three-hour (`180` minute) validation, signal strength horizon, and aging expiry can remain.
* `last_checked_minutes` means a physical investigation/structural check. A distant staffed-camp sound read must leave it unchanged (normally `-1`), so it cannot suppress an investigation for six hours.
* `last_scouted_minutes` is also not written by a distant sound read. A sound is information received remotely, not scouting.
* Actual writers retain their semantics: `record_camp_resource_estimate`, the structural outing stalking check, physical arrival/investigation, and supported/empty signal investigation may write `last_checked_minutes` (and where appropriate `last_scouted_minutes`) at the actual observation time. The existing six-hour `structural_lead_recently_checked` gate then has meaningful effect.

The concrete staffed-read change is in `record_staffed_camp_signal_observations` (`src/bandit_live_world.cpp`, around the `learned` lead construction): assign `learned.last_checked_minutes = sound ? -1 : now_minutes`; leave sound `last_scouted_minutes` at its default and retain the existing non-sound behavior. The comparison path already preserves durable timestamps for an unchanged lead, so a repeated sound packet does not manufacture revisions. Keep the sound `last_seen` timestamp and the three-hour `minutes_after_saturated(last_seen, 3 * 60)` expiration math used by `advance_camp_intelligence_aging`, `structural_signal_strength`, urgent scheduling, and evidence rendering.

Legacy saved leads need a narrow, deterministic compatibility treatment. Old staffed sound leads can contain a bogus `last_checked_minutes = now` because the old writer did not distinguish sound from field/visual reads. On deserialization, if the lead is `sound_signal`, `origin == signal`, `generated_by_this_camp_routine`, `last_outcome == "camp_observer_sound"`, and the value is present, normalize that timestamp to `-1`. Do not clear timestamps on `returned_structural_sound_report` or on outcomes showing an actual physical investigation (`signal_investigation_empty`, physical estimate, or another physical result): those values are semantically meaningful. This avoids treating every historical sound timestamp as trustworthy while preserving evidence of a real check. No schema bump is required because `-1` is already the established absent value and serialization omits it; if implementation cannot safely identify the legacy writer by outcome, preserve the value and document the ambiguity rather than erasing potentially real investigation evidence.

Do not make sound detection itself force a scouting/outing transition. It may create or refresh a suspected sound lead, and the normal scheduler may later choose an investigation based on its ordinary score, route, risk, and cadence. Physical investigation is the event that starts the six-hour cooldown. This gives a plausible response: a remote bang is fresh but uncertain, while a checked location is temporarily known.

## Distinguishing tests

Focused source tests should distinguish these cases rather than only checking field assignment:

1. A committed two-member shakedown with inactive overmap records places both exact pointers, calls canonical loading, yields two active tracker entries with no duplicate IDs, and satisfies the local gate.
2. A placement or activation failure leaves the reservation authoritative and produces no local parley, attack, stalking, or outcome credit; an inactive overmap record alone fails the local gate.
3. A staffed distant sound read creates a `sound_signal` lead with `last_seen_minutes == emitted_minutes`, `last_checked_minutes == -1`, no `last_scouted_minutes`, and the existing 180-minute expiry. Repeating the same packet is unchanged and does not extend `last_seen` or revision.
4. At `emitted + 179` the sound is fresh/eligible; at `emitted + 180` it is expired/aged according to existing logic. A sound at `now` does not make `structural_lead_recently_checked` true.
5. A real structural investigation at time `T` writes `last_checked_minutes == T`; the six-hour gate is true before `T + 360` and false at/after that boundary. A subsequent distant sound does not erase or move `T`.
6. Deserialize a legacy `camp_observer_sound` lead with a positive checked timestamp and verify normalization to unknown; deserialize a returned sound report or a physical-investigation outcome and verify its checked timestamp is retained.
7. A supported returned sound report remains eligible through the existing observation expiry rule and does not acquire a physical-check timestamp merely by being delivered; only arrival/investigation writes one.

## Risks and limits

`game::load_npcs()` has its own map-bound, companion-mission, reality-bubble, and marked-for-death skips. The materializer must treat any such skip as failed local admission rather than compensating with manual tracker mutation. Pointer identity must be checked before and after loading because ID lookup alone cannot prove the exact object survived.

The legacy discriminator is an inference from persisted outcome/origin fields. It is intentionally narrow; ambiguous records should remain conservative and be resolved only by a later physical investigation. Existing sound TTL/strength values and the camp-wide routine cooldown are independent of the six-hour physical-check cooldown and should not be conflated.

