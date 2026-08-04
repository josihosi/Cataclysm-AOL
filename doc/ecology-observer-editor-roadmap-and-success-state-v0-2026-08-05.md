# Ecology observer and overmap entity editor v0

Identity: `CAOL-ECOLOGY-OBSERVER-EDITOR-v0`

Status: ACTIVE / ROADMAP RATIFIED / READ-ONLY OBSERVER FIRST

Parent lane: Phase 4 of `CAOL-HOSTILE-CAMP-ECOLOGY-v1`.

## Goal and proof workflow

One observer handoff starts the prepared evac-shelter playtest with `DEBUG_CLAIRVOYANCE`
already present and records that exact mutation in the transform and run report. Normal starts are
unchanged. With the overmap open, Josef can see, filter, select, pin, and understand live ecology
entities without changing discovery or simulation. The same side-effect-free view emits a compact
artifact payload, so GPT-5.6 can run until one meaningful transition and compare natural changes
with explicitly labelled debug interventions.

The first useful vertical slice is deliberately small: bandit/cannibal camps and structural
dispatches from their current authoritative owner, one overmap overlay/legend/cursor detail path,
and one deterministic artifact snapshot/delta contract. Horde and writhing-stalker adapters join
the same view after their concrete/abstract owner seams are named. The first editor action is an
authoritative dispatch-member casualty operation; no generic entity scripting platform is part of
v0.

## Gate and invariants

- `DEBUG_CLAIRVOYANCE` is the only observer gate. Existing terrain-vision behavior is reused; the
  observer does not add a second hidden toggle. `DEBUG_NIGHTVISION` may remain independently useful
  to existing debug play, but it does not enable this overlay.
- The harness transform may add `DEBUG_CLAIRVOYANCE` only to the disposable prepared character. It
  records `mutation_added`, whether it was already present, and the before/after mutation list.
- Opening/closing or filtering the observer changes display/process-local debug state only. It does
  not change map knowledge, seen terrain, camp intelligence, routes, AI, reports, discovery,
  evidence, or save bytes.
- Filters, selection, pins, deltas, watches, and intervention history stay outside the save. Closing
  the overmap removes observer work; no every-turn scan or saved overlay state is allowed.
- Human UI and JSON consume one shared side-effect-free `ecology_debug_view`. Adapters read the
  authoritative live/abstract owners directly; there are no map notes, duplicate registries, fake
  sightings, player dossiers, or synthesized success facts.
- Every marker carries canonical stable ID, short deterministic alias, entity kind/faction,
  position including z, concrete/abstract owner, loaded state, and natural/debug-intervened
  provenance. Rich health/state/reason/route data is computed only for the selected or explicitly
  exported entity.
- Dead/completed/returned entities disappear on the next query. Multiple entities on one OMT remain
  individually selectable. Stale selection tokens fail closed after identity/generation/owner or
  position changes.

## Exact v0 bounds

- UI query region: the current overmap viewport plus one OMT of padding on the displayed z-level.
- Harness/export region: a 60-OMT square radius around the player, preserving entity z-levels.
- Candidate consideration cap: 2,048 authoritative rows per query.
- Marker/snapshot cap: 256 entities, with the selected entity forced into the bounded result.
- Delta/event cap: 128 transition rows; oldest rows drop first and report dropped count/bytes.
- Sort: `(z, y, x, kind_rank, canonical_id)`; co-located selection cycles by
  `(kind_rank, canonical_id)`.
- Snapshot metadata always reports candidate count, emitted count, each cap, truncation flag,
  dropped count, query/render microseconds, and trace bytes. A cap is not silently presented as a
  complete world.

These are v0 safety bounds, not balancing constants. Raise them only from measured playtest need.

## Shared entity contract

The compact payload includes schema/version, commit/binary/scenario identity when supplied by the
harness, calendar turn and timestamp, player OMT, query region/z, active filters, selected ID, and
deterministically sorted entities. Each entity includes:

- `id`, `alias`, `kind`, `faction`, `omt`, `owner`, `loaded`, `state`, and `provenance`;
- selected detail: party members with stable NPC ID/name/HP/status, source camp, phase, last
  meaningful transition, blocked reason, evidence/reason with age, next eligible update/deadline,
  destination/route summary, and relevant population/interest/target for group owners;
- intervention linkage: `debug_intervention`, intervention ID, confirmation turn, concise before /
  after, and the authoritative operation invoked.

Transition-only deltas are `appeared`, `moved`, `phase_changed`, `hp_changed`, `completed`, and
`died`. Unchanged per-turn history is forbidden. Natural changes use `provenance=natural`; an
intervention delta/event uses `provenance=debug_intervention` and later natural consequences remain
natural while retaining causal linkage to the intervention ledger.

## Authoritative adapters

- Bandit/cannibal camp and dispatch: `overmap_buffer.global_state.bandit_live_world`; site record,
  exact active outing, current simulation owner/handoff generation, member roster, observations,
  route, clocks, and return/casualty authorities.
- Horde: existing overmap monster-group/horde containers and their stable group identity,
  population, interest, target, and current OMT. A horde is a group, not one HP bar.
- Writhing stalker: loaded creature tracker when concrete and the existing overmap monster/group
  owner when abstract. If stable round-trip identity is unavailable in one owner, that state stays
  inspect-only and the missing seam is recorded rather than mirrored.

## Human UI and existing debug-console integration

- Overmap markers use stable distinct symbols/colors for bandit camp, cannibal camp, their
  dispatches, horde, and writhing stalker. An on-screen legend shows symbol/color and active filter.
- One obvious filter action cycles all / camps / dispatches / hordes / stalkers; faction and loaded
  filters remain visible, process-local controls. No second secret toggle is required.
- Cursor details show selected useful facts, not raw serialization. Pin/follow moves only the
  overmap cursor to the selected OMT and never teleports the avatar.
- The existing ImGui console owns play/pause, single-turn, speed, trace, screenshot, report,
  minimized archive, export, and quickload. Ecology adds a view/monitor source and reuses those
  controls.
- At most six watches exist in v0: selected phase change; evidence acquired; exposure/burn;
  casualty; return/completion; no progress by deadline. Each watch can capture, pause, or fail;
  unattended coordinator runs default to capture-and-continue except a contract-defined fatal
  invariant.
- `Record ecology incident` extends the existing screenshot/report/archive path with selected
  snapshot, recent deltas/events, commit/binary/scenario identity, player OMT/time, intervention
  ledger, truncation metadata, and an optional short note.

## Milestones and success state

### O0 - packet, handoff, and no-mutation contract

- [x] Ratify this gate, owner map, exact caps, proof workflow, and deferrals.
- [x] Add one prepared observer handoff transform that idempotently adds
  `DEBUG_CLAIRVOYANCE`, reports the mutation, and leaves ordinary starts untouched. _O0 handoff
  checkpoint: derived fixture preserves inherited `DEBUG_CLOAK`, requests only
  `DEBUG_CLAIRVOYANCE`, reports before/after/added/already-present state, and the launch-only dry-run
  is plan-only. System-Python fixture contract: 125 tests green._
- [ ] Prove observer off/on changes no map knowledge or save bytes beyond the intentional harness
  mutation.

### O1 - shared read-only camp/dispatch view

- [x] Implement one side-effect-free view/query type with stable sort/cap/truncation/provenance and
  selected-only rich detail. _O1 core checkpoint: deterministic retained heap bounds sort work at
  `O(N log 2048)`, emits at most 256, forces a valid selected row, and exposes the 128 event cap
  without owning history._
- [x] Read bandit/cannibal camps and active dispatches from `bandit_live_world`; cover concrete /
  abstract owner, loaded state, source, route, phase, blocked reason/deadline, members/HP, z-level,
  co-location, completion/death removal, and save/load. _Unmaterialized abstract camps remain
  visible; small hostile sites, terminal lost owners, casualties/resolved members, and completed
  parties cannot leave false dispatch markers._
- [x] Add deterministic unit/contract tests for gating, cap/sort, no mutation, stale removal,
  co-location, z-levels, and closed-query zero work. _Exact Mac compile green; focused 5/61 at seed
  `830204929`; five accepted review findings fixed and final AutoReview clean. UI/JSON adapters are
  deliberately O2._

### O2 - overmap observer UX

- [ ] Render stable markers, legend, filters, cursor details, selection cycling, and pin/follow in
  the existing overmap UI under `DEBUG_CLAIRVOYANCE`.
- [ ] Reuse the same view in curses/tiles paths where the existing overmap rendering split requires
  it; do not create divergent truth.
- [ ] Capture one current-build screenshot paired with the exact compact snapshot that describes
  the selected hostile party.

### O3 - horde and stalker adapters

- [ ] Add authoritative horde markers/details and loaded/abstract writhing-stalker inspection,
  including identity/load/unload/z-level/stale-removal tests.
- [ ] Keep unsupported stalker ownership inspect-only with a concrete seam note; do not synthesize
  identity or HP.

### O4 - compact deltas, watches, and incident bundle

- [ ] Emit bounded deterministic snapshot+deltas to harness artifacts and expose a run-until
  operation equivalent to `observe selected-operation --until phase-change-or-anomaly --deadline
  6h --json`.
- [ ] Integrate the six watches with existing step/play controls and transition-only trace capture.
- [ ] Add `Record ecology incident` through existing screenshot/report/archive/export seams.
- [ ] Prove query/render counts/timing, trace bytes, 2,048/256/128 truncation behavior, observer
  closed cost, save-size neutrality, and byte-stability.

### E1 - first authoritative intervention

- [ ] Select a real dispatch member by stable outing generation + NPC ID + owner/location token;
  show before state and require explicit confirmation.
- [ ] Wound, heal, and kill one member through authoritative NPC/outings casualty/death/writeback /
  roster/report paths; never erase a marker or clear an outing as a substitute for death.
- [ ] Emit concise before/after event and intervention artifact with
  `debug_intervention=true`, refresh immediately, and reject stale selection after move,
  load/unload, completion, death, or generation change.
- [ ] Prove one-dead-one-survivor return, both-dead, and wounded-survivor behavior for bandit and
  cannibal camps, or record the exact faction-equivalence seam.

### E2 - type-aware breadth after the stop condition

- [ ] Add safe existing writhing-stalker inspect/wound/heal/kill operations where concrete and
  authoritative; abstract states remain inspect-only without an honest mutation seam.
- [ ] Add horde population reduce/remove and target adjust/clear only through the existing group
  owner; never invent group HP.
- [ ] Extend intervention confirmation/provenance/stale-token tests across these owners.

## Live-use stop condition

Before speculative polish, the tool must be used in the next real Phase-4 row. Josef/GPT-5.6 can:

1. run one documented observer handoff;
2. open the overmap and select/filter/pin a real hostile party;
3. read why it is acting or blocked;
4. run until one meaningful phase/evidence/casualty/return change;
5. make one confirmed authoritative casualty intervention;
6. record one screenshot + compact incident bundle whose deltas distinguish natural from debug
   intervention.

The first target live row is the existing smoke/light/sound matrix. Its pre-fix run
`20260804_214456` remains red/inconclusive; it is not observer proof.

## Explicit deferrals

No full rewind/arbitrary undo, arbitrary phase setter, discovery setter, teleportation, generic
scripting language, remote-control protocol, persistent trails/state, full-world every-turn scan,
parallel registry/simulator/debug console, per-turn dumps, or stimulus tools. A disposable
playtest save is the v0 rollback story. Stimuli may be considered only after the observer, watches,
incident bundle, and first casualty intervention are green, and must use real light/smoke/sound
producers through normal perception.

## Checkpoint receipt template

Each crossed row records: commit; exact behavior; focused command/result; caveat/evidence class;
and next useful row. Mac-only compilation/runtime proof is labelled Mac-only; Linux/Windows claims
require their own compile/test isolation before integration readiness.
