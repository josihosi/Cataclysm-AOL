# Andi handoff: Bandit live signal + site bootstrap correction v0

## Classification and position

- classification: ACTIVE / GREENLIT NOW
- authoritative canon: `Plan.md`
- active queue: `TODO.md`
- success ledger: `SUCCESS.md`
- validation policy: `TESTING.md`
- packet doc: `doc/bandit-live-signal-site-bootstrap-correction-v0-2026-04-26.md`
- audit footing: `doc/test-vs-game-implementation-audit-report-2026-04-26.md`
- repo: `/Users/josefhorvath/Schanigarten/Cataclysm-AOL` on `dev`

## Item title

Bandit live signal + site bootstrap correction v0

## Why this is first

Josef greenlit the full C-AOL debug-correction stack and explicitly told Schani to point Andi at C-AOL on a 30-minute cadence. The audit packet is now closed. Its main finding is that the bandit smoke/light/weather/scout playback layer is useful deterministic rehearsal, but it is not live gameplay until real producers and live dispatch consumers are wired.

This packet is therefore first: make hostile-site ownership and live signal observation real enough that later bandit tests stop proving cardboard.

## Implementation scope

1. Read `Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, `COMMIT_POLICY.md`, this handoff, and the active packet doc before editing.
2. Preserve the audit rule: deterministic/playback tests remain adapter regressions until a live source hook and live consumer path exist.
3. Register bounded hostile-site ownership without requiring the player to walk into spawn/load range first.
4. Carry enough cheap abstract roster/profile/headcount state for dispatch and later lazy materialization without global spawn/perf blow-up.
5. Reconcile materialized NPCs back to the same owned-site ledger so exact-member writeback still works when concrete members exist.
6. Wire at least one real fire/smoke/light observation path into live bandit signal candidates under named weather/time/light conditions.
7. Replace or demote the hard `distance <= 10` live-dispatch gate so ordinary visibility does not impersonate the whole system envelope.
8. Split signal observation/decay cadence from dispatch decision cadence.
9. Add reviewer-readable instrumentation for: empty ownership, no signal packet, below-threshold signal, rejected-by-range, cadence skip, accepted candidate, and hold/chill decision.
10. Add deterministic tests for the range matrix, site bootstrap serialization, signal caps, candidate filtering/scoring, and dispatch accept/reject behavior.
11. Add at least one harness/live proof where a real current fire/light/smoke source creates a live bandit candidate or mark on a real owned-site path, plus one no-signal control.

## Corrected range / cadence shape

- overall overmap AI/system envelope: `40 OMT`
- sustained smoke cap: about `15 OMT`
- ordinary bounty visibility: about `10 OMT`
- confident threat: about `6 OMT`
- hard/searchlight threat: about `8 OMT`
- exceptional elevated light: adapter-bounded inside the `40 OMT` envelope
- movement remains elapsed-time-earned travel credit, roughly `1-6 OMT/day`

Do not revive the older `48/60 OMT` starter lean. The `40 OMT` envelope is the current product law.

## Greenlit stack after this packet

All of these are greenlit in this order. When the active packet is genuinely closed, update canon and continue to the next bounded item unless a real ambiguity, product-risk call, save/perf risk, or evidence-class problem requires Schani/Josef discussion.

1. `Bandit live-wiring audit + visible-light horde bridge correction v0`
2. `Bandit local sight-avoid + scout return cadence packet v0`
3. `Smart Zone Manager v1 Josef playtest corrections`
4. `Basecamp medical consumable readiness v0`
5. `Basecamp locker armor ranking + blocker removal packet v0`
6. `Basecamp job spam debounce + locker/patrol exceptions packet v0`

The closed audit packet was the precondition, not another item to redo.

## Non-goals

- no Lacapult work
- no release publishing, signing, notarization, or repo-role surgery
- no mutation of Josef's real saves/userdata
- no implementation of every queued package inside this first packet
- no deletion of useful deterministic tests merely because they are deterministic
- no claiming gameplay behavior until the live producer/consumer path and evidence class are named
- no broad rewrite unless the active lane truly requires it

## Current checkpoint

- Abstract hostile-site records can exist and persist before local NPC materialization.
- Live dispatch now separates the `40 OMT` system envelope from the ordinary `10 OMT` direct-player cap.
- Raw `fd_fire` / `fd_smoke` near-player fields can create weather-aware smoke packets and refresh live signal marks for owned sites inside the smoke cap; live fire also feeds the ordinary-light adapter with current time/weather/exposure/source detail, but the product proof still needs actual wood/fuel plus ignition through normal in-game mechanics, and the light branch still needs a clean threshold-surviving live proof.
- Selected abstract overmap-special candidates now lazily materialize only the minimum profile-specific scout roster (`2` camp-style, `3` cannibal, `1` small hostile), reconcile those NPCs through the owned-site ledger before dispatch planning, and skip/log failed claims before overmap insertion.
- New harness footing: `map_fields_near_player` save transforms can inject real serialized map fields into copied harness saves, which gives this packet a positive raw-field reader proof without touching Josef's real saves; Josef clarified this is not enough for product proof without the real wood-plus-ignition action chain.
- Latest local gates: `git diff --check`; `rm -f obj/do_turn.o && make -j4 obj/do_turn.o LINTJSON=0 ASTYLE=0`; `make -j4 cataclysm LINTJSON=0 ASTYLE=0`; `make -j4 cataclysm-tiles TILES=1 LINTJSON=0 ASTYLE=0`; `./tests/cata_test "[bandit][live_world]"` -> all 490 assertions in 20 test cases passed; `./tests/cata_test "*bandit_mark_generation*"` -> all 207 assertions in 15 test cases passed; repaired raw-field reader harness `.userdata/dev-harness/harness_runs/20260427_014408/` (current rebuilt runtime `dfa45b1c57-dirty`, source camp seeded at `[0,11,0]` / OMT `[140,52,0]`, raw fields at `[10,0,0]` / `[11,0,0]` on OMT `[140,41,0]`, `|` -> `w` -> `7` three-hour wait, initial `matched_sites=1 refreshed_sites=1 rejected_by_signal_range=0`, later smoke-only rejection and then decay/no-signal). Treat old `.userdata/dev-harness/harness_runs/20260427_011845/` / `.userdata/dev-harness/harness_runs/20260427_013136/` as failed/inconclusive, and do not count direct-player-range or already-active-group dispatch/rejection lines as smoke/fire escalation. Next proof must attempt the real product chain: wood/fuel, ignition source/tool, normal in-game lighting action, visible fire/smoke, safe player placement, several-hour wait, and matched-site bandit signal logs; if automation cannot do one of those controls, park/escalate that exact missing mechanic.

## Success bar

- Abstract hostile-site records can exist and persist before local NPC materialization.
- Live dispatch can reason about bounded signal candidates instead of only the nearby-player envelope.
- At least one real fire/smoke/light observation path produces or refreshes a live bandit mark/candidate under named conditions.
- Range/cadence decisions are visible in logs or artifacts.
- Deterministic tests protect the corrected range matrix and bootstrap/signal logic.
- Harness/live proof reaches the real game path, with one positive signal case and one no-signal control.
- Canon files (`Plan.md`, `TODO.md`, `SUCCESS.md`, `TESTING.md`, and this handoff) match the final state before any commit that claims the packet is closed. Current state is not closed yet: clean live light proof, signal decay, and full hold/chill reasons remain to settle or narrow.

## Testing / evidence expectations

Minimum useful gate for a partial implementation state:

- touched-object compile for changed C++ files where practical
- focused `[bandit][live_world]` / mark-generation filters relevant to the slice
- `git diff --check`
- reviewer-readable source-path note if the slice is only deterministic groundwork

Minimum closure gate:

- deterministic tests for bootstrap/range/signal filtering
- one live/harness positive fire/light/smoke signal proof
- one live/harness no-signal control
- artifact/log excerpts naming site count, signal packet, weather/light modifiers, candidate distance, cap used, and accepted/rejected/hold-chill reason

## Cautions

A passing playback packet is not product truth. A source helper is not product truth. The game proves itself only when the live producer feeds the live consumer and the evidence says which path carried it. Ja, annoying. Also correct.
