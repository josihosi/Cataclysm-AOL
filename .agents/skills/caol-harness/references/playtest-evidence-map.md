# Suite proof and capture map

Use this before preparing an arm, not as a per-turn checklist. Existing run binding, native receipts,
transition stream, selected-NPC trace and retained saves remain the evidence system. No new log
framework or per-scenario recorder. This is a source audit, not native certification of every family.

## Common capture

Bind build/source, run, starting save and setup interventions once. Identify sites and actual operation
generation/member IDs at their real reservation; never reuse scout IDs as presumed raiders. Capture
both factions separately in a shared run and include relevant defenders. R041 local NPC actions,
applied damage, death and sleep edges require the correct selection/window. Abstract travel uses
transition records and actor saves, not the local decision hook. Keep nullable identities/causes honest.

Each compact query returns relevant changes, exact raw handles and coverage: selected actors/time,
last captured event, truncation/orphan repeat bases and display pagination. A query limit is not a
capture limit. A quiet interval is negative evidence only for the selected, supported and retained
scope. An intent is not movement, a shot is not a hit, and a hit is not a confirmed death.

## Every suite arm

| Arm | Minimum evidence for the claimed outcome | Existing route and remaining limit |
| --- | --- | --- |
| 1 ordinary cannibal raid | Typed source admission; same-ID scout/watch/return/report; response decision; individual movement/targets, applied harm and confirmed death/outcome | Signal/transition streams + R041 decisions/damage/death + saves. Preserve each run/build when continuing; no new recorder needed. |
| 1v viewer raid | Same journey, viewer traits recorded; actions/outcomes for every living attacker and defender | Explicitly select defenders too. Viewer perception is not NPC knowledge or ordinary-view proof. |
| 2 and 4r signal-off routine | Native extinguish and measured signal change; real destination/watch/visit, observations, cargo and physical return/report/supply delta | Transitions and retained outing/actor/camp saves. Empty watch is valid, not haul. Gap B below if causal cargo application cannot be retrieved. |
| 3 city roof | Source z/weather/range/LOS; actual party, zombies fought, incoming/outgoing harm and killers; surviving route/floors if reached | Typed signal reads and R041. Death before house is a survival outcome, not a failed stair implementation. Exact sleep cause remains unsupported when null. |
| 3c both signals | Distinct light and smoke reads, observer/site/source/time, both facts in returned report | Existing typed fields include channel, source, range/cap, LOS/weather and lead. One channel cannot stand for both. |
| 3r rural A/B | Fixture diff/manifest, each camp and actor group, approach, real floor transitions, local search, combat or payment outcomes | Same capture as above; saved/native z and path/search rows prove stairs. No city/no interference is fixture scope, not a global no-zombie guarantee. |
| 4 peaceful robbery/debt | Offered demand, actual payment/item/value transfer, debt before/after, paid departure, real return/later visit; defenders' restraint | Native choice receipt + retained transaction/camp/inventory state. Generic Pay acceptance alone is insufficient; gap B. |
| 5 bandit Fight | Actual refusal/fight transition; selected attackers/defenders; fired shots, hits/misses, damage and casualties | R041 handles decisions/hits/deaths. Decision Attack does not establish a fired missed shot; gap A. |
| 6a before-discovery absence | Departure/return times, no new signal versus burning-base branch, actual signal and base/player knowledge | Signal reads/aging, actor/lead saves; gap C for knowledge changes not attributable to a read. |
| 6b outbound; 6c watch | Actual loading/ownership transitions and same actors/route; watch arrival, observations and resolution | Existing handoff/phase transitions plus actor saves; gap D for cursor/position deltas. |
| 6d homeward | Cargo/report carriers, physical camp return, one report/cargo application | Physical-return transitions + saved receipt keys/revisions; gaps B/D if absent from compact query. |
| 6e rally; 6f approach/contact | Response gate/timing, route and local ownership, survivors and base versus avatar knowledge | Decisions/transitions + combat edges; gaps C/D. A legitimate night/capability gate is not a stall. |
| 6g paid departure | Same payment/debt and return continuity across unloading, no duplicate charge | Reuse arm4 evidence; gaps B/D. |
| 6h/6i exposed stalking | Actual reciprocal exposure IDs/positions, burned phase, egress/alternate watch, unloading and eventual report | Existing burn phase transition and assessment state; gaps C/D only for missing causal endpoints. Debug sight alone is not exposure. |
| 6j pursuit | Target/last-known position, real sight/sound/report update and loss, subsequent physical search | Local action target visibility is available but does not by itself prove historical knowledge provenance; gap C. |
| 6k split/edge; 6l casualty; 6m reload | Same identity/HP/life, one simulation owner, cursor/goal/path continuity, no duplicate/resurrected member or double receipt | Handoff transitions, R041 death and retained pre/post actor saves; gap D. |
| 6n vertical crossing | Actual route edges/actor z through real stairs/ramps; signal z remains separate | Existing position/path/search capture + saved map; inaccessible-floor reason, not guessed success. |
| 6o burnout/relight | Fire/fuel/field state before/after and channel admission/aging; memory survives as policy allows | Native interaction receipts, local observations/saves and typed signal reads; no per-turn fuel dump. |
| 7 lamp-only | Electrical baseline/on-state, light read/report, absence of smoke in measured capture, faction-specific response | Existing typed signal records; missing lamp baseline is setup, not a logging defect. |
| 8 patrol/defense | Actual duty/shift/locker state, release/recruitment, chosen target and combat; separate peaceful collector interval | Selected camp NPC decisions work. Duty/roster/inventory snapshots establish changes; gap B/C only if decisive cause is missing. |
| CLI controls | Ordered accepted actions, pending/exact-ID recovery, requested/completed count, real turns/moves/HP and owner, terminal cleanup | Existing R043 audit/receipts. Keep control result separate from packaged-run classifier; gap E. |

## Bounded implementation gaps for the existing tooling owner

Do not implement all possible telemetry upfront. Supply one tested existing query per evidence family
first, against retained records where available. A saved before/after result with causal receipt is
sufficient when it answers the question. Add only the missing fields/edge at its real commit point.

**A — Fired attacks that miss: confirmed compact-view gap.** `cockpit_evidence._TRACE_EVENTS`
includes action/damage/death/sleep, not a shot event. R041 damage emits only HP decreases. Existing
`Character::fire_gun` in `src/ranged.cpp` emits character ranged-attack events; expose retained native
attack events if they preserve actor/time/target/weapon/fired count. StatsTracker aggregates cannot
replace the exact interval. If the raw stream lacks that event, adapt the actual successful discharge
boundary into the existing opt-in recorder: actor, origin z, weapon type, actual rounds fired, target
ID or aimed tile, turn. Record misfire/refusal distinctly; do not count intent as discharge or infer
hit from firing. Damage/death stay in existing events. No projectile trajectory dump. Test miss,
hit, empty/misfire, selected/unselected and opt-out; retain raw handles in the same reader.

**B — Transaction/cargo/duty: query first; commit-edge gap conditional.** Inspect
`live_bandit_commit_paid_return` in do_turn.cpp and `apply_return_packet`/cargo receipt application
in bandit_live_world.cpp. Bind native trade/choice receipt to saved demanded/surrendered/debt,
cargo/supply and application keys. If insufficient, emit one scoped committed state delta using
existing transition infrastructure: site/operation/generation, commit/receipt key, transfer category,
actual value/quantity and before/after balance. Item detail stays in retained inventory evidence;
no full inventory per tick. Failed/repeated application must not emit a second committed transfer.
For patrol, use the actual duty-to-defense/roster commit with actor and prior/new role and trigger;
do not log routine decisions twice if existing selected-NPC rows plus state already answer it.

**C — Knowledge and burn causality: query first; no omniscience.** Existing staffed signal reads
already contain observer, channel/source, weather/range/LOS and lead provenance; reuse them.
At a changed pursuit knowledge or reciprocal-burn commit, if current records omit the basis, expose
actual observer/subject, perceived/last-known position and z, observation time/source kind, lead or
report revision and resulting state. The code already holds reciprocal exposure reads in do_turn.cpp
and the committed burn effect in bandit_live_world.cpp. Never fill an unseen target with the current
avatar coordinates from global state. Record a dropped/lost basis once, not every failed sight check.
Test seen/unseen targets, both factions, debug viewer separation and exposure across reload.

**D — Handover/cursor deltas: confirmed generic-event field limit; saved state may suffice.**
`record_live_transition` carries site/operation/generation/epoch/owner/phase/IDs, but not the physical
pair positions, route cursor, cargo or rejection detail needed to explain all split-pair failures.
First expose a compact correlated transition + retained saved-owner/actor projection. If a transient
crossing is lost between saves, extend existing handoff/abstract-resume/physical-return events at
commit with previous/next owner and cursor, route position/goal z, affected actor positions/life and
receipt key. For a repeated rejection emit first/last/count and concrete rejection reason only on
change. No all-world polling. Test split pair, boundary corner, survivor/zero survivors, save/reload,
vertical edge and idempotent return/cargo against actual adapters. Preserve accepted earlier proofs.

**E — Clean exit classified as failure: source-supported harness bug.** startup_harness.py
computes expected_clean_terminal_exit before deferred scenario terminalization from an already
finished cockpit cleanup status alone. Reconcile the SAME run/step's requested finish, exact process
exit and final cleanup before producing the final feature verdict, preserving prior genuine feature
errors. Use one shared predicate/finalization path rather than another special-case status string.
Never make every process exit green: crash/nonzero/unrequested exit, wrong binding, incomplete
cleanup and missing evidence stay failures/unknown. Reproduce the R043 deferred-clean-exit case and
negative controls; rederive a corrected report with provenance while retaining the original. Native
accepted action receipts remain valid; a final report repair is not new gameplay proof.

## Retention, usability and scope

Use existing run-bound opt-in for detailed additions and existing transition collection guard. No
unconditional DebugLog analytics, new database or per-scenario schema. Filtering before serialization
and no added RNG/gameplay reads with side effects; opt-out silence and same-seed behavior parity.
Reuse common identity/turn/position fields; do not attach every optional field to every event.
Extend existing reader filters/projection and rollover preservation alongside any producer. Preserve
referenced repeat bases and important transfer/handover/combat edges; report capture loss explicitly.
A longer 24h/dual-faction run needs a measured capture-size check, not a blind fixed-budget increase
or truncation reclassified as inactivity. Simultaneous selection must not merge faction outcomes.

Implementation belongs in existing tooling assignments; these are proof-surface corrections, not
changes to gameplay acceptance. No new FS gameplay amendment is required merely to expose facts
or repair report classification. If an observation proves a gameplay defect, keep its separate scoped
FS route. Validate with retained query examples, focused producer/reader/retention controls, then
one short Luna native use for the newly supported family. Reuse that evidence across related arms;
never make every playtest repeat the logging acceptance matrix. Unknown exact sleep cause and
monster/stalker AI selection remain explicit unsupported boundaries, not reasons to add speculative
instrumentation now. Monster damage endpoints already support combat-survival proof.
