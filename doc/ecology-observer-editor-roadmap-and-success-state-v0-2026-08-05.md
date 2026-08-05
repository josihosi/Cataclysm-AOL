# Ecology observer and overmap entity editor v0

Identity: `CAOL-ECOLOGY-OBSERVER-EDITOR-v0`

Status: ACTIVE / FIELD GATE GREEN / O4 SAVE-PERF NEXT

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
  mutation. _Two live strategies in run `20260805_055304` remain inconclusive because first-save
  rewrite and matched lazy-overmap/serializer variance survive mutation normalization. Exact hashes
  are preserved; retry only through the existing O4 performance/save-growth harness._

Execution-order exception: this deferred serializer-variance proof and O3's missing identity seam
do not justify a third bespoke retry or fake IDs. The practical party-first stop condition makes
O4a the current row.

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

- [x] Render stable markers, legend, filters, cursor details, selection cycling, and pin/follow in
  the existing overmap UI under `DEBUG_CLAIRVOYANCE`. _O2 code checkpoint `a7b844d100`: the normal
  overmap owns the visible controls; category, faction, and loaded-only filters plus selection/pin
  survive a reopen only in process memory and reset across worlds. Fast travel and specialized
  overmaps do no observer queries._
- [x] Reuse the same view in curses/tiles paths where the existing overmap rendering split requires
  it; do not create divergent truth. _Both renderers consume the same bounded marker projection;
  the Trace export and selected monitor serialize that same query contract. Exact Mac tiles build,
  20/176 ecology assertions, 9/1,116 console assertions, keybinding JSON, diff check, and final
  AutoReview are green._
- [x] Capture one current-build screenshot paired with the exact compact snapshot that describes
  the selected hostile party. _Run `20260805_055304` pairs a 2,560x2,880 overmap screenshot with
  the exact 1,893-byte Trace export for natural camp `BC-E75C82`; hashes and the detached-launch
  caveat are recorded in `ecology_observer_live_receipt.json`._

### O3 - horde and stalker adapters

- [x] Add one opt-in bounded mobile-owner shape to the shared view and compact snapshot without
  enabling unavailable sources. _Checkpoint `a4e4cb5c22`; schema 2 passes exact Mac link,
  `[ecology_debug]` 23/232, `[debug_console]` 9/1,116, and `git diff --check`. The provider receives
  region, selected ID, and cap; invalid or colliding IDs fail closed; absent providers report both
  mobile filters false. One AutoReview pass found the absent-source and collision defects and both
  were fixed._
- [ ] Add authoritative horde markers/details and loaded/abstract writhing-stalker inspection,
  including identity/load/unload/z-level/stale-removal tests. _Deferred: current C-AOL and fetched
  upstream `7f6b236556` expose position-keyed horde/group owners and temporary loaded-monster
  tracker identity, none of which survives movement plus concrete/abstract transfer._
- [ ] Keep unsupported stalker ownership inspect-only with a concrete seam note; do not synthesize
  identity or HP. _The seam is recorded in `TechnicalTome.md`; mobile sources stay disabled until
  an authoritative persisted identity exists._

### O4 - compact deltas, watches, and incident bundle

- [x] Add O4a selected phase-change watch with an immutable world/ID/generation/owner token,
  transition-only `appeared`/`moved`/`phase_changed`/`hp_changed` records, exact 128-row retention,
  and capture-and-continue or existing-controller pause behavior. Ambiguous disappearance is an
  anomaly, not invented death/completion. _Behavior `759e0851bd`; exact Mac delta 3/334, capture
  9/34, console 9/1,116, and full ecology 26/566 after one review/fix pass. Live preflight
  `20260805_074635` selected natural camp `BC-E75C82`, but the bridge focused SDL helper window
  `22148` instead of render window `22114` and could not arm the ImGui watch. Permissions were
  green; the bounded receipt is non-credit and the field-tool gate remains open._
- [x] Emit bounded deterministic snapshot+deltas to harness artifacts and expose a run-until
  operation equivalent to `observe selected-operation --until phase-change-or-anomaly --deadline
  6h --json`. _Behavior `13cbeeb072`: one compact watch-session JSON combines the latched watch
  result with the deterministic 128-row delta payload; default six-hour deadlines and terminal /
  fatal outcomes remain machine-readable without OCR._
- [x] Integrate the six watches with existing step/play controls and transition-only trace capture.
  _The same checkpoint adds exactly six typed presets, capture/pause/fail policy, a bounded 1-72
  hour deadline, and `Arm + play` through the existing controller. Typed evidence, status-only
  casualty, completion/death, timeout, and structured stale results are retained; unchanged turns
  add no trace. Incident schema 2 includes the latched watch result. Exact Mac watch 6/63, incident
  6/80, ecology 40/792, console 10/1,120, tiles/non-tiles compile, tiles link, and the bounded
  review/fix/recheck are green. Private console-session latching has compile/surrounding-suite proof
  rather than a direct unit seam._
- [x] Add `Record ecology incident` through existing screenshot/report/archive/export seams.
  _Behavior `541932daa5`; exact Mac incident 5/71, console 9/1,116, harness 126/126, runtime
  link, and Python syntax are green after one combined review/fix pass. The action synchronously
  revalidates the selected owner and atomically publishes exact-byte compact JSON plus the existing
  game screenshot into the harness run directory. Live field use remains uncredited until the
  readiness-gate loop reaches watch arm and intervention._
- [x] Let the selected ecology watch/incident workflow operate without a second global-debug toggle.
  _Adapter `15e01c1e64`: `DEBUG_CLAIRVOYANCE` renders only ecology snapshot/watch/incident plus the
  existing Step/Play footer, with `A` arm, `P` play/pause, `.` step, and `R` incident. Other console
  tabs/actions remain behind global debug mode. Exact Mac console 10/1,120 and release tiles link
  are green. Field attempt `20260805_091051` identified the old toggle and remains non-credit; its
  post-build relaunch hit a case-sensitive quit modal before adapter proof._
- [x] Keep the selected watch alive across one authoritative editor interaction and provide a real
  dispatch-ready handoff. _Field bridge `22004574a1`: `I` queues the same selected-overmap editor
  outside ImGui, suspends the console during blocking menus, and returns to the same watch. The new
  launch-only fixture is byte-identical to natural producer run `20260804_121729`, contains a real
  schema-8 two-member local structural handoff, adds only `DEBUG_CLAIRVOYANCE`, and preserves the
  producer's yellow whole-probe caveat. Exact Mac compile/link, console 10/1,120, intervention 2/60,
  all ecology 33/697, harness 126/126, dry-run, and fresh read-only review are green. Live use is not
  credited until the clean field run records the intervention/incident pair._
- [x] Execute the complete field workflow and preserve one compact incident pair. _Runtime
  `648a509cc9`, run `20260805_101713`: selected `BD-374153`, armed the watch, stepped one turn,
  confirmed authoritative NPC 4 kill with NPC 5 surviving, captured natural appeared then
  debug-intervention HP delta, published the 4,099-byte JSON/PNG pair, and immediately refreshed
  the overmap provenance. Query 2/2/26 us, trace 2,047 bytes/no truncation, permissions, identities,
  and exact hashes are in `ecology_field_gate_receipt.json`. Non-credit attempts `095314`/`100813`
  isolated the shared SDL3_image success-contract defect; `f997bbd368` defers capture and
  `648a509cc9` normalizes SDL2/SDL3 return values. Live save-byte neutrality remains the next O4
  performance row, not a claim of this field run._
- [ ] Prove query/render counts/timing, trace bytes, 2,048/256/128 truncation behavior, observer
  closed cost, save-size neutrality, and byte-stability.

### E1 - first authoritative intervention

- [x] Select a real dispatch member by stable outing generation + NPC ID + owner/location token;
  show before state and require explicit confirmation. _Behavior `1081f6f6a0`; `I` opens the
  selected entity inspector, and mutation re-resolves world/entity/generation/owner/authority,
  exact outing cursor, member ID/OMT, loaded/alive state, and HP after confirmation._
- [x] Wound, heal, and kill one member through authoritative NPC/outings casualty/death/writeback /
  roster/report paths; never erase a marker or clear an outing as a substitute for death.
  _Wound/heal operate on the concrete NPC. Kill calls normal `npc::die` then `game::cleanup_dead`,
  and reports success only when existing local-handoff casualty writeback records the NPC. Abstract,
  unloaded, non-structural, and non-local parties remain inspect-only._
- [x] Emit concise before/after event and intervention artifact with
  `debug_intervention=true`, refresh immediately, and reject stale selection after move,
  load/unload, completion, death, or generation change. _A bounded 32-row process-local receipt
  feeds same-turn overlay provenance, existing monitor trace, and O4b incident serialization. A
  prevented death/writeback failure is retained as a labelled failed intervention; delayed natural
  changes are not relabelled debug. Exact Mac release build/tests: intervention 2/60, all ecology
  33/697, console 9/1,116, release tiles link, JSON/diff checks. The installed AutoReview launcher
  was absent; one manual review/fix pass found and closed the two provenance/receipt defects._
- [ ] Prove one-dead-one-survivor return, both-dead, and wounded-survivor behavior for bandit and
  cannibal camps, or record the exact faction-equivalence seam.

### E2 - type-aware breadth after the stop condition

- [ ] Add safe existing writhing-stalker inspect/wound/heal/kill operations where concrete and
  authoritative; abstract states remain inspect-only without an honest mutation seam.
- [ ] Add horde population reduce/remove and target adjust/clear only through the existing group
  owner; never invent group HP.
- [ ] Extend intervention confirmation/provenance/stale-token tests across these owners.

## Field-tool readiness gate - before any behavior ledger

Do not begin or expand a Phase-4 behavior ledger merely because an O4 implementation row exists.
First execute one documented field-tool dry run against a naturally observed camp or dispatch.  The
operator must be able to select it, use the existing step/play controller with a selected-operation
watch until a meaningful transition, anomaly, or deadline, automatically capture the exact selected
snapshot plus bounded deltas and incident bundle, make one explicitly confirmed authoritative member
intervention, and see the immediate refresh with query/trace/save-growth evidence.  A plan or unit
test alone does not satisfy this gate.  If a step lacks an honest UI, harness, or authoritative-owner
seam, implement only the smallest missing adapter before starting the ledger.  Reuse the existing
console/harness; do not add generic state setters, teleports, stimulus generators, scripting, or a
second scenario framework.

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
