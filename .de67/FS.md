# CAOL seeded overmap AI campaign and native long soaks — Functional Specification

Status: Frozen — 2026-11-22
WEC: `.de67/WEC.md`
Source baseline: `Cataclysm-AOL | dev | 563dbc5687321fc8912d32e4007581f9615fc85d | Mac working tree refreeze inspected 2026-11-22`

## Document authority

The seeded-campaign WEC remains unchanged. Its SHA-256 is
`9e15d3739f7073cd842adcecca3f42f0c25924801411b7e3f9f8d0a5b8820220`.
The owner explicitly directed this Phase-2 refreeze to preserve that entire frozen campaign and
merge the complementary interactive long-soak intent into this canonical FS. The seeded clauses
and historical acceptance retain their scope; the three new R-SOAK claims remain open. This FS
specifies both tracks from those owner decisions and the actual Mac callers.
It does not assert that the campaign has been implemented or run. A source finding is not a native
reproduction. Historical acceptance below retains only its original scope and gives no credit to
the new campaign or soak claims. The user owns economic balance and any material change to intended behavior.

The old WEC is an archive, not additional campaign intent. The old FS snapshot remains available at
`.de67/archive/FS-2026-11-22-97d51dc1257-0f1f8c330e37.md`, SHA-256
`0f1f8c330e376a7ba870688b1cbe530a00bd1727e37d910068d6dd377122b4b1`.
The corresponding WEC snapshot is `.de67/archive/WEC-2026-11-22-97d51dc1257-47cd7f00cc96.md`,
SHA-256 `47cd7f00cc968362bc92bd5bdd6214b67fd7978cbfd4cb54be54613ff68f8f0f`.
Both remain on this Mac outside the phase checkpoint. The WEC archive matched the prior active WEC.
Reversing only the six FS withdrawal-line changes committed in `876a6c48e226` exactly reproduced
the archived FS from the prior active FS; there was no unexplained archive difference.
Those committed withdrawals remain withdrawn. Existing durable acceptance and historical claim/slice
identities are preserved below; superseded execution prose is not a new campaign instruction.

Status markers: `[x]` denotes scoped prior evidence, not a campaign or soak pass. A line beginning
`- [ ] 🔴 R-CAMPAIGN-...` or `- [ ] 🔴 R-SOAK-...` identifies missing or unproved work.

## Ordinary native play and the two complementary tracks

A player lights and tends a brazier, waits through a night, hears something approach, opens a
door, retreats through a town, and later returns to the same camp. These ordinary actions must
exercise the actual simulation: what each actor perceived, where it moved, what interrupted its
job, and what survived a save or a reality-bubble crossing. A quiet three days is useful elapsed
time, but it does not establish a scout journey, a raid, a horde or a return that nobody observed.

The **seeded campaign** below retains its complete contract: independent scenario/setup and game
RNG seeds, roughly 1,000 pilot runs, expansion toward 10,000 and beyond when measured coverage and
cost justify it, connected transitions, physical invariants, and fresh-process failure replay.
The **interactive long-soak track** adds twelve mixed worlds, each targeting three actual game
days, with varied ordinary player decisions and separately measured opportunities and outcomes.
Neither track substitutes for the other. Automated invariant checks and experiential observations
share a finding/replay handoff while keeping their different proof scopes visible.

For live play, use the existing [C-AOL playtest skill](../.agents/skills/caol-harness/SKILL.md).
The owner's chosen route is a native Luna player with a Terra chaperone. Luna owns launch and
game input; the chaperone checks starting conditions, current build/data binding, source and
receiver eligibility, actual time and retained evidence. This names the requested play route,
not a second coordination policy. The skill owns operating details; this FS owns the observable
outcomes and evidence limits. The game remains authoritative over actions and simulation state.

The player uses `tools/openclaw_harness/play_cli.py --session SESSION` in the Mac checkout.
`look` exposes the current native World or menu; `controls` reads retained controls and
permissions without advancing time. `act ACTION` uses the current advertised action/target,
while `move` and `wait` express bounded ordinary movement and native waiting with interruption
handling. An action's successful reply is already the next observation. A pending request is
continued with `collect`, never submitted again; `resume` recovers an outstanding recorded
request rather than replaying input. `inspect`, `messages`, `evidence` and `performance`
read retained facts and timings without granting a stale frame new input authority. Follow the
advertised controls out of menus before moving or waiting. Native quicksave, save/quit, main-menu
exit and new-process reload are distinct boundaries, each requiring its own observed result.
See [live controls](../.agents/skills/caol-harness/references/live-operation.md).

The [World Workshop](../.agents/skills/caol-harness/references/world-workshop.md) is the existing
setup route, via `tools/openclaw_harness/startup_harness.py world`:

| Operation | Useful result and its limit |
| --- | --- |
| `inspect WORLD` / `inspect WORLD --zoom --map` | Reads saved terrain, homes, populations and positions at 40-OMT radius or 20-OMT zoom. It neither generates terrain nor advances play. The displayed nearest footprint distance is not automatically the AI's anchor/member eligibility distance. |
| `prepare SOURCE [RECIPE] DESTINATION` | Creates a fresh disposable copy with declared terrain, homes, items and requirements; retains parent/recipe/readback identity. A placed home still needs its actual native staffing and relevant knowledge state checked. |
| `scratch WORLD --dry-run`, then `scratch WORLD` | Checks the selected build/source, then launches a separately owned disposable native world through the registry. Use the returned session and explicit pinned build receipt when required. A successful launch is not gameplay proof. |
| `history` / `history --kept --family FAMILY` | Finds prior results and retained checkpoint identity without assuming the old conditions fit a new question. |
| `keep RUN_ID --reason REASON [--checkpoint CLOSED_WORLD]` | Retains a useful searchable example and, when needed, its exact closed checkpoint. Keep the active reproduction or continuation; release redundant successful-run output after its concise result is durable. |

The implementation routes are `world_cli.py::run` to `world_inspect.py::inspect_world`,
`world_prepare.py::prepare_world/verify_requirements`, `world_scratch.py::launch_scratch`
and `world_examples.py::history/keep`. Preparation refuses an occupied destination or changing
source; scratch uses the registry/build binding and an exclusive writable profile. After native
initialization and save, `world inspect CLOSED_WORLD --check PREPARATION_JSON` rechecks the
original requirements. Unmet or unknown requirements remain visible setup gaps.

For the next round, the owner selected **current Mac dev plus the verified prevent-death fix**.
At this refreeze, dev and origin/dev are `563dbc5687321fc8912d32e4007581f9615fc85d`; the fix is
still a local change to `data/json/effects_on_condition/example_eocs.json` with the untracked
`tests/debug_prevent_death_test.cpp`. Before execution, pin the actual current source/dirty
patch, executable, data/config and save identities together and prove that the selected runtime
contains the fix. Neither a branch name nor the old `caolpd` receipt proves that a new runtime
has it. Product integration/build publication remains separate work from this FS-only checkpoint.

Declare debug interventions and their time explicitly. `DEBUG_LS` and
`DEBUG_PREVENT_DEATH` permit sustained observation while retaining ordinary visible contact;
they are modified survival conditions, not proof of unassisted survival. The verified fix routes
`EOC_event_character_dies_test` through `PREVENT_DEATH` rather than the already-committed
`character_dies` event. Existing focused and native lethal-fire/bleeding replay evidence in
`prevent-death-finding.md` covers that encountered route, not every possible death mechanism.
`DEBUG_NODMG` is a documented exception when needed for continued observation; record when
enabled and qualify damage, reflection, combat and retreat conclusions accordingly. Invisibility,
clairvoyance or altered sight can confound perception and must be declared; scored visibility
segments require the ordinary relevant sight state. Never relocate, replace, resurrect or rewrite
tested actors, their orders or clocks to rescue a scored journey. A diagnostic intervention starts
a separately labelled control and cannot repair the original segment's verdict. Avatar relocation
to load a stalled offscreen party likewise cannot count as its natural progress.



## Functional contract

A campaign invocation reads an explicit scenario revision, resolved setup recipe, setup seed,
game RNG seed and action policy. It validates the source/build/data/save binding before execution.
If they match, it establishes disposable initial conditions and follows the actual game callers
through travel, interruption, return, settlement and the next decision. If they do not match,
it records a setup/binding failure without gameplay coverage. At an observed event, it records the
actual identities and state before performing a scheduled action. An event that never occurs is
a visible coverage miss, not a successful execution of that transition.

A run compares actions with earned knowledge and physical authority. The operation owner, each
body's executor, and settlement owner are distinct facts. It records the first contradicted
invariant and its inputs; subsequent symptoms do not replace that first event. It replays a failure
in a fresh process with the same resolved setup and game seed. Where predecessor runs matter, it
retains the shortest reproducing sequence found and labels isolation failure. Repeats are grouped
by the first invariant and meaningful state, without losing their distinct setups.

Campaign execution, replay and load measurement belong on the exact existing Mac development
workspace. Run size is empirical: approximately 1,000 pilot attempts, then approximately 10,000
and beyond when reached coverage and measured cost justify it. These are planning proposals,
not pass thresholds. Week-scale execution is permitted, not a requirement to consume a week.
The campaign must resume and yield resources without losing completed results or pending seeds.
Only isolated disposable worlds or disposable reference-save copies may be changed.

## Project language and fixed rules

Use **bandit**, **cannibal**, **Mi-go**, **camp**, **tower**, **reality bubble**, **admission lease**,
**operation**, **physical authority**, **settlement**, **active** and **frozen** as the WEC uses them.
Camp economies include each faction's own rules, not a shared invented economic model.
Retain the repository's C++ production/tests and Python harness conventions.

Seeds vary terrain, floors, route length, separation, injuries, obstacles, resource stocks,
staffing, competing leads, signal geometry, production/return/casualty timing, player crossings,
and event-relative actions. A setup seed and a game RNG seed are independent recorded inputs.
A matched comparison changes only its declared factor. Fixed across seeds are identity and lease
authentication, one payer per movement interval, real perception, valid physical access, finite
resource accounting, once-only settlement, persistent earned progress and truthful failure reporting.
Current balance, activation rules, population ceilings and production intervals remain the oracle;
starvation, dispatch frequency and replenishment concerns are findings for an owner balance decision.

## Current code map and inspected evidence

- `tests/test_main.cpp`: the Catch listener resets game RNG from `rngSeed()` before a test.
  `--rng-seed-fuzz N` repeats `session.run()` with successive seeds and reuses the loaded process.
  It stops on test failure or observed debug error. It does not independently generate setups,
  report reached events, resume a campaign, deduplicate failures or prove fresh-process replay.
  Zero is treated as unspecified seed; campaign replay must record a concrete effective nonzero
  native seed, rather than passing zero and claiming determinism.
- `tests/scout_pair_seed_sweep.py` is absent on this Mac. The WEC's Windows 1000–1399 results,
  24-versus-six setup sampling correction and seed 48151 result are prior reported evidence only.
  The named local production controls exist in `tests/bandit_live_world_test.cpp`:
  “structural shared route starts physical admission at the loaded boundary”,
  “structural staged arrival leaves the distant peer on shared travel”, and
  “mobile player partial edge and reversal preserve unsaved scout return”.
  The local file has a pre-existing 122-line diff affecting signal/payment/mixed diagnostics,
  not the absent Windows sweep runner. That diff is an inspected input, not Phase-2 work.
- `src/do_turn.cpp`: the overmap NPC scheduler prepares travel, reconciles bodies and returns,
  services signals and site decisions, then prepares per-member shared movement and physical entry.
  `materialize_live_bandit_structural_handoffs()` commits an existing staged admission first;
  retained abstract resumes deliberately skip the old placement transaction. Pending-entry handling
  rechecks activity, generation, epoch, body position and goal before granting an admission lease.
  `begin_authenticated_structural_admission`, `commit_staged_live_bandit_pair` and dematerialization
  are competing crossing paths to cover, not interchangeable fixture shortcuts.
- `src/bandit_live_world.h/.cpp`: `active_outing_state`, `structural_admission_state`, per-member
  coarse work, return receipts and cargo keys preserve the operation. `begin_structural_admission`
  validates a cursor and constructs a candidate; `stage_structural_admission_member` binds/rolls
  back an individual body before publishing that candidate. Generic owner transitions reject
  active staged admission. Save/load validates these structures.
- `src/overmap_party_travel.h/.cpp`: `prepare_transit(work &, actors, policy)` chooses routes and
  paid edges. `actor_input` binds actor ID, operation generation and lease epoch; `transit_policy`
  supplies active milliseconds and movement phase. A local phase yields immediately; a blocked
  or local edge returns before coarse edge funding. Adapters commit actual bodies. Route caches
  and retry state are not alternate position owners.
- `src/bandit_live_world.cpp`: `advance_camp_supply` uses the home's active elapsed minutes and
  living-member accounting; a frozen initialized home returns without consumption.
  `claim_finite_resource_units` validates operation, generation, target and resource revision;
  replay returns already-applied, stale or depleted without a new debit. The arrival path commits
  a candidate world, turns claimed units into cargo, and begins return. `credit_structural_return_cargo`
  consumes current supply first, then checks the cargo generation/key and caps credit at camp capacity.
  `record_structural_member_physical_return` and `apply_return_packet` are distinct settlement paths.
- `src/do_turn.cpp::live_bandit_encounter_goods_pool` and its commit callers inspect actual speaker,
  receiver, cargo access and physical goods. Physical payment is not equivalent to structural
  supply cargo; verify both using their own receipts and custody rules.
- `src/migo_tower_native.cpp`: `receive_signals`, `begin_response`, `prepare_coarse_step`,
  `refresh_activation` and `service_operations` connect observation, reservation, shared travel,
  physical navigation, search, return and settlement. `refresh_activation` evaluates home and
  participants separately against the existing 40-OMT XY range. `src/migo_tower_ai.cpp` persists
  operation/member state and individual retreat intent.
- `src/migo_population.cpp/.h`: `site_record` owns seed, member ID/epoch, slot/local/abstract/dead
  representation, reservations, returns, ceiling, birth sequence and production elapsed time.
  `advance(world_turn, active_elapsed, source_exists, place)` rejects duplicate time, unknown or
  destroyed source and uncertain occupancy; it clears production credit at ceiling, retains at most
  one due event on placement failure, and publishes a fresh identity only after placement succeeds.
- `src/migo_population_native.cpp::service_loaded`: settles real local health/returns independently
  of a pod, then services active homes only when the source is in the loaded map and geometry exists.
  It supplies at most one active turn only if `last_service_world == now - 1`. The production
  call in `src/do_turn.cpp` reaches this function. This is a concrete coverage risk against the
  existing active-site-time rule, not evidence that unloaded production has been proved.
- `src/npcmove.cpp::npc::move` and operation-duty execution own local human actions;
  `src/monmove.cpp` calls `migo_tower::plan_local` with native flight/target facts and
  `migo_tower::move_local` in the actual monster action path.
  `src/physical_navigation.h` separates read-only route/geometry facts from
  `move_npc_into_loaded_area`, authenticated geometry acquisition and paid `commit_door`.
  A route read or cache hit alone grants no action or time credit.
- `src/savegame.cpp` writes/reads `bandit_live_world`, `migo_populations` and `migo_towers` in global
  state; `src/savegame_json.cpp` persists NPC projection leases. Native and saved payload copies,
  map spawn slots, `overmapbuffer.cpp` materialization and native death notifications must agree.
- `tools/openclaw_harness/startup_harness.py` already has source/runtime binding, registry launch
  validation, isolated-world setup, event-stream reading, semantic observation/action and cleanup.
  `scenario_registry.py` validates proof/setup contracts. `writable_run_owner.py::own_writable_root`
  supplies exclusive writable-root ownership. Extend these existing boundaries for live campaign
  cases; a new runner must not invent a bypass to an owned world or a second launch authority.

`doc/overmap-ai-field-guide.md` is the maintained navigation/debugging reference. Its historical
source statements remain dated. The campaign adds evidence, not a new production owner. If a
campaign repair changes its answers about crossings, payment or Mi-go response, the affected
claim's deliverable includes that passage updated with the actual caller, proposed/delivered
status and outstanding native verification. No current caller change is authorized by this FS's
source inspection alone.

## External research sweep

No external lookup was needed: the consequential uncertainties are in these local production
callers and the availability of the Windows pilot. An upstream implementation cannot establish
this fork's staged admission, current dirty test behavior or earned native active time. Resolve
those through connected local proof. Do not infer the missing Windows test contents from its name.

## Mechanistic requirements

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-REPLAY-S001 claim=R-CAMPAIGN-REPLAY -->
### Independent seeds, resumable runs and honest coverage

- [ ] 🔴 R-CAMPAIGN-REPLAY — A Mac campaign runs and resumes independently replayable native scenarios with honest reached-transition accounting.
  - Code gap: `tests/test_main.cpp` varies one game seed in a reused process; no local seeded
    campaign entrypoint resolves independent setup randomness or resumable run results.
  - Required mechanism: introduce a Python campaign entrypoint beside the existing test/harness
    tools, using `tests/overmap_ai_seed_campaign.py` as a proposed starting location, not a binding
    filename. It resolves a versioned scenario plus setup seed into explicit parameters before
    launch, and passes the recorded game RNG seed independently to the existing native test or
    harness route. Resolved recipes include the initial save/world, mods/options and initial time.
    Store the RNG algorithm/version for setup generation and the resolved values so later changes
    to the generator cannot silently change a replay. Event schedules contain a predicate, relative
    action and actual firing record. An unmet predicate records a miss; a rejected premise records
    setup failure; a violated game invariant records a gameplay finding. Keep those outcomes distinct.
  - State: the campaign owns only pending/claimed/completed attempt records, resolved recipes,
    coverage and replay/grouping metadata. A run identity binds scenario revision, both seeds,
    resolved recipe, executable/data identity and attempt. Persist claim-before-launch and result
    publication atomically; a resumed campaign reconciles the owned process/world before retrying
    an interrupted attempt. It never starts a second writer for an uncertain live attempt or
    overwrites a completed result. Cancellation preserves pending work. Unknown exit is incomplete.
  - Coverage: count attempted/completed runs, distinct resolved setups, reached events and misses
    separately by family and transition. Deduplicate setup parameters rather than assuming distinct
    seeds produce distinct setups. Keep fixed regressions alongside randomized variants. Compare
    setup generation at fixed game seed and game randomness at fixed setup in separate controls.
  - Proof: interrupted pilot -> same pending set and durable completed results -> safe resume;
    fixed recipe/native seed -> fresh-process reproduction; changed setup seed at fixed native seed
    -> recorded setup differences; an unreachable event -> miss, never transition credit. The
    Windows 24-versus-six sampling discrepancy is the explicit false-green counterexample.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-REPLAY-S001 claim=R-CAMPAIGN-REPLAY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-FAILURES-S001 claim=R-CAMPAIGN-FAILURES -->
### Detect, replay and group the first broken invariant

- [ ] 🔴 R-CAMPAIGN-FAILURES — Failure reporting proves its own sensitivity and retains the smallest useful reproduction.
  - Code gap: the seed-fuzz loop stops on error and prints a seed, but does not retain a resolved
    setup/action sequence or classify repeated failures. Existing event and identity bindings
    can support evidence; a process exit alone cannot identify the first wrong game transition.
  - Required mechanism: at each checked decision record its preconditions, operation/body identity,
    lease/epoch, relevant time/accounting, action and outcome. On the first broken invariant retain
    source commit plus relevant dirty identity, executable/data/config hashes, scenario revision,
    both seeds, resolved setup/save, executed actions and the last valid/first invalid observation.
    Distinguish harness/setup failure, coverage miss, expected refusal, crash/hang and gameplay
    contradiction. A timeout supplies an incomplete/stall finding with its observation boundary,
    not automatic proof of a specific gameplay defect.
  - Replay starts a fresh native process. If the failure disappears, test the recorded predecessor
    sequence without hiding the standalone miss. Reduce setup/action/seed sequence only while the
    same first invariant still reproduces; report shortest found, not mathematically minimal.
    Group by invariant plus faction/phase/ownership/meaningful state. Preserve affected setup IDs
    within the group; distinct causal evidence must be separable even if messages match.
  - Proof: a disposable known-failure control in the reporting/test layer, clearly labelled and
    absent from product behavior, must be detected, replayed and grouped before large-run evidence
    is trusted. A corresponding unbroken control passes; two different invariant breaks remain
    separate; interrupted evidence publication does not produce a completed/pass record.
    Native setup and fixture errors are diagnosed against the real caller before proposing a fix.
  - Retention: successful disposable saves/logs/captures are removed once concise coverage/result
    records are durable. Keep active reproductions and required continuations only; release their
    bulky artifacts after disposition, retaining commit/run identity and reproduction steps in the
    existing task context. Do not create a second DE67 acceptance or historical-proof system.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-FAILURES-S001 claim=R-CAMPAIGN-FAILURES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-HUMAN-TRAVEL-S001 claim=R-CAMPAIGN-HUMAN-TRAVEL -->
### Split scout crossings, local orders and complete human journeys

- [ ] 🔴 R-CAMPAIGN-HUMAN-TRAVEL — Seeded bandit and cannibal journeys preserve physical authority and earned progress through crossings, interruptions, return and the next decision.
  - Code gap: current staged-admission and return tests exercise fixed routes; the referenced
    Windows sweep and its sampling correction are not available locally. Older materializer-only
    controls cannot reach retained abstract pairs because production intentionally bypasses that
    placement transaction. Treat their reported setup failures as fixture incompatibility until
    the scheduler's current admission path itself contradicts a valid premise.
  - Required mechanism: parameterize connected scenarios around the current overmap scheduler,
    shared travel, staged physical entry, native movement and return consumers. Preserve existing
    fixed controls. Start from authenticated real members, establish the seed's declared initial
    separation/injury/terrain, then require subsequent movement and crossings to use production.
    Record setup intervention separately. Never mark a direct relocation, forced owner transition,
    forced report or manufactured settlement as journey progress.
  - Decision oracle: `prepare_transit` may fund a distant legal edge; a local edge yields to physical
    admission. `stage_structural_admission_member` may bind one qualified body while the outing is
    abstract. That body cannot receive abstract interval funding again; an eligible offscreen peer
    continues its own travel. Only the qualified surviving set commits local operation ownership; authenticated deaths do not require a nonexistent partner.
    A refusal retains source identity/position/earned work and a truthful reason. Reload and player
    reversal preserve each body's wounds, goods, leases and watch progress. Watching begins at real
    arrival; return/casualty receipts do not fabricate a completed watch.
  - Variations: road/field/forest, floor and route length, separated starting positions, injury,
    obstacle opening/closing, player crossing between members and reversing with exactly one
    staged, save/reload before/after the same boundary. Include a temporary blockage and a genuinely
    impossible route. For local execution vary doors, sightlines, fire/hazards and perceived enemies
    immediately around the next native decision. Native danger can interrupt a mission; resumption,
    rerouting, refusal and individual withdrawal must follow actual new facts and physical ability.
  - Proof: actual one-staged observation -> event-relative reversal/save -> same actors with one
    payer per interval -> qualified commit or justified refusal -> survivors return/settle -> next
    legitimate decision. Controls cover stale cursor, failed bind/rollback, absent peer, incapacitated
    body, temporary recovery and impossible route. A whole-pair owner flag alone is insufficient.
    Bind `npc::move`, physical navigation and actual perception where local behavior decides progress;
    calling a travel helper or assigning a destination does not prove that native action.
  - Starting references: the three named staged/mobile tests in the code map, “scout unreachable
    routes retry less often than their scheduled updates”, “outbound scout orders recheck a changed
    next step”, “completed reconnaissance survives actual return cohesion and full game reload”,
    and `tests/overmap_party_travel_test.cpp`. Reconcile seed 48151 only after the exact incoming
    pilot or an honestly labelled local successor has been established; do not claim a Windows rerun.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-HUMAN-TRAVEL-S001 claim=R-CAMPAIGN-HUMAN-TRAVEL -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-BANDIT-ECONOMY-S001 claim=R-CAMPAIGN-BANDIT-ECONOMY -->
### Bandit supply, resource and payment custody

- [ ] 🔴 R-CAMPAIGN-BANDIT-ECONOMY — Seeded bandit economic journeys conserve finite resources and physical property and credit the correct home once.
  - Code gap: resource, supply, cargo and payment controls exist separately; a large connected
    seeded collection/payment -> carrier travel -> home -> settlement -> next-decision campaign is unproved.
  - Required mechanism: scenarios invoke the existing camp decision/arrival, physical goods/payment
    and return callers. Independently account for resource revision/depletion, camp consumption,
    physical item custody and return credit. After arrival, `claim_finite_resource_units` admits only
    the current issued job and matching resource revision. Stale/duplicate requests cannot debit
    twice. Structural cargo and physical payment keep their separate application keys and paths;
    replay cannot turn one into another or credit another camp. Revalidate actual goods access
    at payment commit, not just when displaying a demand.
  - Variations: supply near current decision thresholds, staffing, competing leads and camps,
    depleted/contested sources, carried/pocket/vehicle/basecamp goods under the existing eligibility
    rules. Interrupt between collection, physical transfer, movement, home arrival and settlement;
    include reload and an expired/stale operation. Observe capacity clipping and consumption
    explicitly rather than calling any unequal before/after stock a conservation defect.
  - Proof: finite source revision + real collector -> one debit/cargo -> same carrier or truthful
    loss -> authenticated return -> one appropriate credit -> next decision. A duplicate receipt,
    inaccessible item, stale reservation and wrong-home receipt must refuse/no-op as appropriate.
    Starting controls: finite-resource atomic/idempotent and save-contract tests, supply catch-up,
    “encounter payment rechecks native carrier and cargo access”, “paid home return settles its
    exact consumed opportunity despite camp lead refresh”, and split-return receipt tests.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-BANDIT-ECONOMY-S001 claim=R-CAMPAIGN-BANDIT-ECONOMY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-CANNIBAL-ECONOMY-S001 claim=R-CAMPAIGN-CANNIBAL-ECONOMY -->
### Cannibal pressure, forage, raid and survivor settlement

- [ ] 🔴 R-CAMPAIGN-CANNIBAL-ECONOMY — Seeded cannibal journeys explain member availability and distinguish provisional survivor arrival from final economic settlement.
  - Code gap: the shared human state has faction-specific policies and physical raid callers;
    fixed controls do not prove the WEC's varied long economic cycles and interrupted returns.
  - Required mechanism: generate a valid cannibal home and named roster under existing supply and
    readiness rules. Drive the camp's routine/hostile decision and actual travel/raid/return caller,
    retaining the reason each member is available, wounded, away, reserved or dead. Observe shared
    supply accounting without assuming bandit extortion or payment rules apply to cannibals.
    `record_structural_member_physical_return` may acknowledge one survivor before the operation
    settles; `member_return_receipts`, casualty resolution and the final return/cargo consumer
    determine later credit. A missing partner is not silently a casualty or a complete party.
  - Variations: both sides and equality of actual supply-pressure boundaries, reserves, wounded
    and away members, successful forage, empty source, raid withdrawal, partial return, confirmed
    casualty, independent frozen-home/active-party states and reload at first survivor arrival.
    Retain legitimate reports while frozen; no frozen-time work or invented catch-up is credited.
  - Proof: eligible named members -> actual dispatch/forage or raid -> interruption/withdrawal ->
    observed provisional return -> exact final settlement -> correct next camp decision. Include
    no-eligible-party and empty-source controls, duplicate first-arrival/terminal receipts and
    reopening a temporary route. Use current readiness/supply predicates rather than new balance
    constants. Start with admitted cannibal frontier journey, night-rally, partial-return,
    source-signal-after-physical-return and first-survivor provisional-receipt tests.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-CANNIBAL-ECONOMY-S001 claim=R-CAMPAIGN-CANNIBAL-ECONOMY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-MIGO-RESPONSE-S001 claim=R-CAMPAIGN-MIGO-RESPONSE -->
### Mi-go perception, response and individual retreat

- [ ] 🔴 R-CAMPAIGN-MIGO-RESPONSE — Seeded Mi-go response journeys preserve the same real residents and each member's intent through local and distant execution.
  - Code gap: `migo_tower_native.cpp` connects existing signal, reservation, navigation, search and
    return mechanisms, but the randomized connected combinations are unproved.
  - Required mechanism: begin with a legitimate tower/encampment and source, then let `receive_signals`
    admit actual observer evidence and `begin_response` reserve eligible population identities.
    Follow `prepare_coarse_step` and `service_operations` through physical search, native engagement,
    return/death and the next tower decision. The native monster owns combat and immediate escape;
    the member's retreat latch survives handoff/reload and later directs it home. It does not change
    an engaged peer's intent or convert a safe obstacle dodge into retreat.
  - Variations: signal position/visibility, floors/vertical access, spreading fire, separated members,
    obstacles on both sides of loading boundaries, one retreating member, incapacitation/death,
    player reversal and save/reload. Capture tower versus individual observations separately;
    unseen targets or casualty causes cannot become tower knowledge merely from global state.
  - Proof: real received signal -> exact resident reservation -> paid travel -> observed local
    search/engagement -> one member retreats while peer remains engaged -> same identities settle
    or remain truthfully unresolved. Negative controls include occlusion, unavailable residents,
    stale epoch, blocked entry and no physical witness. Starting tests include native boundary
    handoff/refusal, retained positive outward/home segments, actual-death physical witness/home
    communication, response reserve fallback and independent actual-body intervals.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-MIGO-RESPONSE-S001 claim=R-CAMPAIGN-MIGO-RESPONSE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-MIGO-ECONOMY-S001 claim=R-CAMPAIGN-MIGO-ECONOMY -->
### Mi-go population and active-site brood time

- [ ] 🔴 R-CAMPAIGN-MIGO-ECONOMY — Matched native journeys verify population identity and brood production in loaded, active-but-unloaded and frozen intervals.
  - Code gap: `service_loaded()` skips an unloaded source and supplies only consecutive service
    time. `site_record::advance` can account supplied active time but cannot manufacture it.
    `migo_native_brood_service_placement_and_destroyed_source_are_once_only` seeds a nearly-due
    event and proves placement/duplicate/destruction boundaries, not a full earned active cycle.
    Prior `R-REL-MIGO-POPULATION` acceptance stays scoped; these are new comparison obligations.
  - Required mechanism: the campaign compares the same legitimate source, roster, elapsed active
    time and seed under continuous loaded service, active-but-unloaded service, frozen time and
    reload interruption. Measure actual activity clocks and production elapsed/last-service values
    at the native caller. The fixed correctness oracle is the existing intact verified source's
    seven active-day event below its persisted ceiling, with no frozen-time credit, no first-load
    retroactive births and no birth from destroyed/unknown source. Do not redefine active time as
    loaded time to make the current branch pass. Report the first native discrepancy with its cause.
  - Independently track slot/local/abstract/dead representation by site/member ID and epoch.
    `site_record` alone allocates births/reservations; the physical adapter must successfully retain
    or place a body before the birth commits. Eligible returned bodies become available once;
    dead identities never respawn. Living away members still count toward ceiling. Capacity-full
    time earns no replacement bank; blocked placement retains at most one due event. Source
    destruction clears production and cannot select a replacement pod on reentry.
  - Variations: near ceiling and near an event, available/away/wounded residents, return and death
    timing, blocked/fire-covered home placement, pod destruction, pre/post service save/reload,
    first discovery versus load-first, and independent home/party activation.
  - Proof: matched observed active intervals -> native service input -> ledger/placement result ->
    actual one fresh body or justified refusal. Run at least one earned long cycle rather than
    deriving all coverage from injected near-due state. The latter remains a useful labelled
    boundary control. Include duplicate service turn, stale epoch, failed placement then recovery,
    ceiling plus casualty, source destruction plus reload and frozen-home party settlement.
    An oracle failure creates reproducible work; this claim does not preselect an unproved offscreen
    placement implementation or authorize a balance change. A repair must preserve these same
    owners and physical provenance, with the matched comparison as its production proof.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-MIGO-ECONOMY-S001 claim=R-CAMPAIGN-MIGO-ECONOMY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAMPAIGN-COVERAGE-LOAD-S001 claim=R-CAMPAIGN-COVERAGE-LOAD -->
### Measured expansion, mixed load and a useful campaign result

- [ ] 🔴 R-CAMPAIGN-COVERAGE-LOAD — The campaign expands by reached native coverage and measures cost while preserving actual faction work.
  - Code gap: existing scenario/metrics facilities and the one-seed loop are not a campaign inventory
    of connected states, independent active/inactive loads or resumable week-scale execution.
  - Required mechanism: inventory maintained scenario families from their actual caller/proof route,
    beginning with split scout crossing, human resource/payment return, and Mi-go response/brood.
    For each family record available setup, intended/reached transitions, negative controls and
    unknowns. Add other overmap AI families and interactions from real callers as coverage becomes
    measurable; an unsupported family remains an explicit gap, never a claimed exhaustive result.
    This coverage inventory is campaign data, not a second product specification.
  - The Mac pilot measures native run/setup/replay cost and failure-reporting validity before scaling.
    Preserve long economic cycles and complete next decisions, not just many cheap short cases.
    Select execution concurrency using actual CPU/memory/storage pressure and the Mac's active
    workload; yield at durable boundaries and preserve safe owned continuations. No fixed worker
    count or universal time/latency pass threshold is introduced.
  - Mixed cases independently vary active sites/parties, player travel and inactive-world population.
    Record completed signals, travel edges, handoffs, returns and stock/birth updates with turn
    latency distributions/spikes, simulated-time throughput, wall/CPU time, memory and stalled or
    repeated work. Compare matched factors and label setup, save/load, UI waits and external
    contention separately. Fast turns with no actual dispatch or frozen actors prove no active
    workload affordability. Timing alone cannot establish an overmap AI cause.
  - Proof: an initial pilot with honest setup diversity/misses and known-failure sensitivity ->
    measured scaling decision -> resumable connected coverage including long cycles and mixed
    loads -> concise results, reproducible failures and remaining gaps. Compare fixed active load
    with different inactive populations, then fixed inactive population with different active load.
    Every performance comparison also demonstrates the intended actors completed their work.
    Historical Windows passes and existing soak coverage do not count as new Mac campaign runs.
<!-- DE67:DFS-SLICE:END id=R-CAMPAIGN-COVERAGE-LOAD-S001 claim=R-CAMPAIGN-COVERAGE-LOAD -->

## Complementary interactive long-soak requirements

These are new open obligations alongside the eight unchanged seeded-campaign claims. The
existing task notes in `.de67/task-logs/R-REL-V2-SOAK-SETUP-001/` provide bounded starting
evidence: `setup-matrix.md`, `three-day-soaks-findings.md` and `prevent-death-finding.md`.
Their earlier release target and ten-slot proposal are historical setup inputs; the owner now
requires current dev with the verified fix and the following twelve-world complement.

<!-- DE67:DFS-SLICE:BEGIN id=R-SOAK-SETUP-S001 claim=R-SOAK-SETUP -->
### Twelve qualified mixed worlds with real encounter opportunities

- [ ] 🔴 R-SOAK-SETUP — Twelve independently owned mixed worlds expose the declared native opportunities on the pinned current-dev runtime.
  - Code gap: the Workshop, registry and source-bound launch exist, but the earlier pilots do not
    establish the twelve required mixed worlds or all their receiving actors. A scenario name,
    terrain footprint or declared horde does not prove a live populated opportunity.
  - Required mechanism: reuse existing prepared recipes and closed checkpoints through the
    Workshop; verify bandit and cannibal homes and a Mi-go tower in every mixed world after any
    necessary native initialization. Record actual staffing/identities, terrain, starting time,
    weather, elevation, source geometry and initial actor knowledge. Preparation visits/contact
    remain visible and cannot masquerade as unseen first contact.
  - World coverage: three bandit-focused worlds, three cannibal-focused worlds, two Mi-go-focused
    worlds, two horde/writhing-stalker-focused worlds and two flesh-raptor-focused worlds. Vary
    city, forest and field contexts across this set. Human and Mi-go remote sensing/travel cases
    include both 15- and 20-OMT source-to-receiver footprint geometry, plus actual anchor/member
    distances and vertical/visibility eligibility. Predator and horde contact is local; the remote
    distances are not invented local-contact requirements.
  - Fixed opportunities are declared before play; ordinary player choices within them vary.
    Each world includes meaningful movement, waiting and day/night exposure; the set covers
    light, smoke, sound and genuinely quiet comparison periods. Random exploration does not
    replace the required opportunities. Record chosen actions and any setup/action seeds; if a
    seed is unavailable, say so and retain the resolved setup/actions instead of claiming seeded
    determinism for a model-driven session.
  - Proof: bound build/data + prepared readback -> actual populated actors and source opportunity
    -> native first-contact boundary. A no-eligible-receiver or absent-local-actor control remains
    unqualified, never a behavior failure. Dry-run/launch alone cannot close this claim.

The coverage allocation is a test question, not a promise that every creature performs on cue:

| Worlds | Principal observed opportunity |
| --- | --- |
| Bandit 1–3 | Remote eligible signal, outbound scouting, search/watch, robbery or truthful refusal, physical return and subsequent decision; include terrain detour and bubble crossing. |
| Cannibal 1–3 | Remote eligible signal, dispatch/raid, native doors/stairs or terrain constraints, local danger/withdrawal, survivor return and subsequent decision. |
| Mi-go 1–2 | Sustained qualified light/smoke opportunity, exact receiving tower/residents, native approach/search/combat or refusal, individual retreat and return; retain the seeded track's separate long brood-cycle proof. |
| Horde/stalker 1–2 | Real local horde contact and stalker opportunity, interaction under mixed pressure, after breaking sight and after leaving the load. Simultaneous presence requires its own observation. |
| Flesh raptor 1–2 | Real skirmishing/approach, attack and retreat opportunities in contrasting open and constrained/crowded terrain, with native behavior and local actor identity observed. |

The retained horde edge checkpoint in `three-day-soaks-findings.md` reached 19 zombies and one
shrieker after one ordinary north step and a short safe wait on a separate copy. That supports
the horde contact route; a simultaneous stalker in that bubble remains unverified. The Mi-go pilot
confirmed sustained brazier fire and tending, but did not qualify the receiving tower's signal
eligibility or prove that the encountered shooter originated there. Neither observation closes a
full lifecycle. The projectile out-of-bounds finding and its post-warning reload reproduction stay
parked at the owner's direction; this complement does not commission that repair.
<!-- DE67:DFS-SLICE:END id=R-SOAK-SETUP-S001 claim=R-SOAK-SETUP -->

<!-- DE67:DFS-SLICE:BEGIN id=R-SOAK-JOURNEYS-S001 claim=R-SOAK-JOURNEYS -->
### Three actual game days with observed native transitions

- [ ] 🔴 R-SOAK-JOURNEYS — Twelve interactive native soaks target three game days each and report elapsed time separately from the behavior actually reached.
  - Code gap: one earlier performance world reached 72h01 without a confirmed horde or active
    hostile party; the Mi-go world stopped at 21h20. These are bounded prior observations, not
    twelve completed mixed-world journeys.
  - Required mechanism: play ordinary native actions through the existing cockpit, using current
    advertised input authority and preserving each world's owned process/save. Target twelve
    worlds in waves of three to four as requested, adjusting simultaneous advancement to measured
    Mac capacity and active work. A smaller active subset or an interrupted continuation preserves
    the twelve-world outcome; neither skipped worlds nor shorter duration silently counts as done.
  - Measure actual start/end turn and game time and segment boundaries. Three days means at least
    72 hours of observed advancement from the declared start, excluding setup time. Prompt/input
    waits and wall-clock time do not advance that total. An interruption, crash or early stop
    reports achieved duration and remaining time truthfully.
  - Follow signal -> observation/admission -> travel -> local interaction -> return/settlement ->
    next decision when the opportunity occurs. The existing physical-ownership/perception and
    faction economy rules above remain the oracle. For stalking, hordes and raptors, retain the
    concrete opportunity, pursuit/attack/loss/return states actually exposed by their native paths;
    do not fabricate camp-style settlement for a creature that has none.
  - Track declared opportunities, observed events, misses, expected refusals and unexplained
    behavior independently per world. A completed duration with no qualifying interaction is
    elapsed coverage only. Replenish a fire through ordinary tending and observe fuel/weather/
    source strength during the interval; expired light or smoke cannot support a missed-response
    defect. Read-only observer evidence may establish internal facts but does not grant actors
    knowledge or advance their mission.
  - Before diagnosing a stall, capture the same actor/operation IDs, positions, local/abstract
    authority, admission state, activity/frozen state, last progress, intended destination, actual
    elapsed game time and known cadence/cooldown. Follow the relevant production path in
    `do_turn.cpp`, `bandit_live_world.cpp`, `migo_tower_native.cpp` or the local movement caller.
    Observe a sufficient eligible decision opportunity; a cooldown, native hazard, pending input
    or inactive home is not automatically stalled travel. No arbitrary new timeout defines a bug.
  - Proof: per-world pinned start -> ordinary actions -> actual duration and event sequence ->
    verified checkpoint/exit or named continuation -> separate elapsed and behavioral verdict.
    Save/reload retains identity and earned state where exercised; record whether resume/reload
    preserves, clears or changes a symptom. A reload recovery never erases the original observation
    and cannot supply missing pre-reload coverage.
<!-- DE67:DFS-SLICE:END id=R-SOAK-JOURNEYS-S001 claim=R-SOAK-JOURNEYS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-SOAK-FINDINGS-S001 claim=R-SOAK-FINDINGS -->
### Shared finding handoff and interpretable performance

- [ ] 🔴 R-SOAK-FINDINGS — Interactive results produce reproducible findings and controlled timing evidence without overstating seeded proof or native load.
  - Code gap: the earlier timing intervals had differing actors/load, overlap and incomplete native
    stage attribution. A repeated message count did not identify its emitter or actor count.
    Existing cockpit evidence/performance and Workshop history are the starting readers, not a
    new parallel reporting or acceptance system.
  - Required mechanism: keep a concise bug/finding list with run/world, source/build/data/dirty
    identity, scenario/setup or checkpoint, available seeds, actor/operation IDs, last ordinary
    action, first symptom with turn/time, expected versus observed behavior, evidence reference,
    intervention/confounds and current disposition. Distinguish observation, inferred cause,
    confirmed reproduction, setup defect and known parked finding.
  - Retain the nearest useful native precontact/pretrigger quicksave and confirm its saved turn;
    a stale snapshot is labelled stale. Retain a symptom-state save only when it adds evidence.
    A crash retains the last usable save and crash facts. Replay from an isolated copy in a fresh
    process using the pinned baseline; record action differences and whether reload clears,
    preserves or changes the failure. No seed exposed means no seed-based replay claim.
  - Feed useful reproductions into `R-CAMPAIGN-FAILURES` with the same first-invariant/faction/
    phase/ownership grouping. An experiential finding becomes an automated regression only after
    the native trigger and oracle are actually encoded and demonstrated. Automated failure counts
    and soak observations remain separate, cross-linked evidence in the common result.
  - Compare a quiet baseline, a verified busier interval, and a matched sparse/after-leaving-load
    interval with actual turns advanced, wall time, latency distribution and individual spikes,
    active creature/party counts, player movement and available stage receipts. Preserve native
    completed work alongside timing. Account for debug traits, loose fuel/item piles, saves,
    menus, loading, simultaneous advancing games and host contention; repeat suspicious parallel
    timing in isolation before assigning a simulation cause. Missing measurements stay unknown.
  - Performance alarms trigger inspection, not a new acceptance threshold. `play_cli.py`
    performance/evidence readers retain bound receipts; native owners supply actual simulation
    time. A quiet world with no verified horde cannot be called mixed-horde capacity proof, and a
    grouped wail count cannot be called an actor count or an established message bug.
  - Proof: one usable finding handoff links pretrigger state, first symptom and fresh-process
    outcome (including an honest non-reproduction); per-world results separate duration,
    opportunities, behavior, bugs and performance limits. Matched timing plus completed native
    work supports only its observed workload. When no bug is found, report that bounded result;
    the seeded known-failure control still owns reporting-sensitivity proof.
  - Finish through native save/quit and verify the owned process/helper exit before disposal.
    Preserve only active continuations, useful examples and reproductions; remove disposable
    output when the concise result and necessary evidence are durable.
  - World 2 dispatch repair (006, open). At minute 7800 the remote home branch of
    `prepare_live_bandit_home_admission` loaded one detailed map per occupied home OMT before
    calling body catch-up. Four loads included 555.240 ms of map generation in replay 005.
    [Detail and same-ID movement](task-logs/R-SOAK-FINDINGS-dispatch-detail-replay-005/replay-result.md)
    · [Caller and proposed behavior](../doc/overmap-ai-field-guide.md#offscreen-home-readiness-at-dispatch).
  - In dispatch preparation, check actual site membership, at-home state, physical location and
    activation ownership. For a stale, eligible offscreen body, derive shelter, bedding/rest,
    seating and temperature from camp profile, overmap terrain and area weather. Model ordinary
    residents as able to free themselves and seek a bed. Apply the estimated rest only during
    the eligible active interval. Use the individual's wounds, equipment, needs and effects;
    avoid leaving a new sleep effect on an awake departing scout. Omit cosmetic ground blood.
    Settle each elapsed interval once. Skip current or suspended intervals as appropriate.
    Repeated checks, including after roster materialization, cannot charge that time again.
    The remote body branch makes zero detailed-map load/save calls, including on exceptions.
    In-bubble bodies use their ordinary local update. Scope the change at admission; independent
    recall, rendezvous, expedition travel and general NPC catch-up retain their existing owners.
  - When death needs the real map or a death script, persist the unresolved transition on the
    same NPC before invoking death side effects. Preserve cause, available source/killer,
    position, possessions, and the exact applied body/effect boundary. Exclude only that actor
    from selection; peers remain eligible. A pending actor with lethal HP has no finalized
    death credit. Repeated admission and save/reload retain the pending transition without
    rerolling it. On real local resolution at that position, before relocation, invoke ordinary
    death/prevention once. A survivor resumes remaining work from the saved boundary; a death
    settles roster, possessions and applicable corpse once. Until a visit, that actor may remain pending indefinitely.
  - Verify through the dispatch caller with counters sensitive to cached and cold loads/saves,
    ordinary and exceptional bodies, local arrival, prevention and actual death, and reload
    before and after resolution. Compare uninstrumented whole-turn and body costs from the same
    pre-dispatch checkpoint and follow both parties' IDs through subsequent offscreen movement.
    The earlier 34.841 ms body total and replay 005 timings are observations, not speedup claims.
<!-- DE67:DFS-SLICE:END id=R-SOAK-FINDINGS-S001 claim=R-SOAK-FINDINGS -->

## Competing systems and override direction

- **Campaign attempt and world custody.** Readers: campaign results/replay and existing harness
  validation. Writers: campaign attempt journal and the existing writable-run owner. One claimed
  attempt owns one writable disposable world; native game/save writers retain exclusive custody.
  Resume reconciles that binding before action. Setup generators cannot change an executing world.
  A duplicate launch or stale source binding fails before gameplay; completed results are immutable.
- **Human mission versus body movement.** Readers: overmap scheduler, local NPC duty/perception,
  assessment and return consumers. Writers: `active_outing_state`, authenticated admission/lease
  transactions, native bodies, shared paid-edge work and generic NPC travel. Mission state owns the
  job; staged/native bodies own their physical interval. Native danger overrides immediate action
  without deleting earned mission facts. The shared coarse owner yields at local edges and for an
  admitted body; its eligible distant partner continues. Activity/generation/epoch, origin and
  current goal bind transfer. Failed bind rolls back, stale requests refuse and repeated same
  staging is unchanged. Pair commit and eventual settlement are separate transactions.
- **Human economy.** Readers: routine pressure/dispatch, lead estimates, demand UI and return logic.
  Writers: home supply accounting, global finite-resource claim owner, physical item containers,
  member-return receipts and operation cargo/return application keys. A demand does not transfer
  items; an actual payment does not pre-credit home. Exact issued job/resource revision owns a
  debit; authenticated arrival/return owns its corresponding credit. Candidate-state commits and
  idempotent keys prevent partial or replayed credit. Provisional survivor arrival cannot close an
  unresolved peer. Independent home and party active clocks prevent frozen intervals paying work.
- **Mi-go residents and intent.** Readers: tower capability/reservation, native combat/search,
  population service and debug projection. Writers: population ledger, exact map slots/payload
  adapters, physical death/health facts and member mission intent. One site/member/epoch identifies
  a body across representations. Newer physical death wins over stale abstract state; return
  requires a valid body at home. Native escape can latch only that member's retreat; neither an
  abstract step nor a peer's order clears it. Population bookkeeping does not disclose an unseen
  death cause to tower intelligence. Replay cannot reserve, return or birth the same identity twice.
- **Active time and brood.** Readers: production/dispatch/movement and campaign oracle. Writers:
  activation evaluation supplies admitted intervals; the population owner records bounded progress
  and native placement verifies physical existence. Loaded service is currently a competing filter
  on earned home time, so the comparison observes both. Frozen time never earns work; necessary
  settlement remains possible without reopening production. A destroyed source is terminal.
- **Proof versus setup/diagnostics.** Observers read existing event/native/save state; they never
  advance game truth merely by querying it. Direct fixture state is permitted to establish declared
  preconditions, not to manufacture the claimed transition. Native setup controls, component tests
  and complete live journeys retain their distinct evidence scope. External AI responses, if used,
  require recorded configuration and replayable input/output or an explicit nondeterminism limit;
  a game seed alone does not control a model service. Deterministic rule tests may declare the
  existing LLM option disabled, but that must be fixed and visible in their compared setups.

## Acceptance and proof

The eight seeded-campaign claims and three complementary soak claims remain open. Each proof route above follows:
`declared initial conditions -> authoritative caller -> observed transition -> invariant result -> bound result/reproduction -> pass, expected refusal, miss or finding`.
A campaign result may be useful because it finds a real bug. That is not acceptance of the bug's
repair. Keep report sensitivity, reached coverage, native behavior and subsequent repair verdicts
separate. No unsupported numerical balance target, fabricated actor knowledge, forced movement or
synthetic settlement can make a seed pass.

Existing C++ controls are starting points, not claimed reruns. New runner verification must exercise
actual native invocation, interruption/resume and fresh-process replay on this Mac, in addition to
focused reporting/generator tests. Existing platform compatibility is preserved; this campaign's
selected execution scope is Mac. No game, test suite, campaign or soak was run during this Phase-2 refreeze.

## Source identity and freeze record

- Status: Frozen — 2026-11-22, refrozen after the owner-authorized FS merge, current native-route
  inspection, source-hash and internal-consistency recheck.
- Validation: all 90 slice markers pair uniquely; all eight seeded-campaign slice blocks and the
  entire 79-slice historical appendix are unchanged from the pre-merge FS. All 69 checked claim
  lines and 24 delivery receipt lines are retained. The eight campaign and three soak claims remain open.
  No campaign or native behavioral result is claimed by this freeze.
- Inspected source HEAD: `563dbc5687321fc8912d32e4007581f9615fc85d`, branch `dev`, upstream `origin/dev`.
  The previous seeded freeze was published at `4f39936189f`; its original inspected product
  baseline was `876a6c48e226b27db90c9d443f1b6d13fa6c698d`. Historical evidence is not relabelled.
- Relevant pre-existing dirty inputs: `tests/bandit_live_world_test.cpp`,
  `data/json/effects_on_condition/example_eocs.json`, harness build/source-binding, registry,
  writable-owner implementation/tests and harness guidance. All remain owned by their prior work.
- User-owned choices: exact incoming WEC; Mac-only campaign; existing workspace/live-work
  preservation; independent setup/game seeds; meaningful connected coverage; week-scale allowance;
  faction-specific correctness; economic balance left open. The explicit complementary owner
  intent adds twelve mixed three-day worlds, ordinary Luna play with Terra observation, the
  World Workshop route, declared debug conditions, separate duration/coverage and a common
  finding/replay handoff. Current dev plus the verified prevent-death fix is required at execution;
  this checkpoint contains only FS changes and leaves that product patch with its existing owner.
- Evidence-implied refinements: current staged admission replaces the old materializer-only fixture
  assumption; native brood caller is a new matched-comparison obligation; missing Windows runner
  is a local availability gap, not a disproved prior result. No new product vocabulary or balance.
- Freeze exception: named-proof closure, evidence-implied nonmaterial clarification or uniquely
  implied same-contract append-only expansion preserves stable identities, accepted evidence and
  acceptance strength and requires immediate refreeze. Material behavior, balance, vocabulary or
  competing designs return to the user and DE67 Phase 2.

Seeded-source hashes below were rechecked at this refreeze (SHA-256; bind the working bytes,
not a presumed clean binary). Additional inspected soak-route hashes follow the original list:

- `src/do_turn.cpp`: `8778a8da2a396ec4c0e3ce0b1aa543e58996c1985bb9b7d6ba2ae5e141715e24`.
- `src/bandit_live_world.cpp`: `392251128e07453a79b8394ecf65c99f423bc9821a780252dfffe9461455627e`.
- `src/bandit_live_world.h`: `7f592ffac03b94d40de65d6792b5ea8d62433c843952431d08b5384b3474cbde`.
- `src/overmap_party_travel.cpp`: `c0b4d843e8664198c935a4a2ece26032479071bd88306857602141db2cda03bd`.
- `src/overmap_party_travel.h`: `23e69e165ef910be6da5ae00adb361161a3fdb4d79338d2914a8b097a6d799cb`.
- `src/migo_population.cpp`: `41e3cb71b938ea34840868662b2f6324ba9338ca1a4e634d71af89fcfda26f60`.
- `src/migo_population_native.cpp`: `2d07eaf824cd266c9f62a8e89079d4735a019649ba21b0900b1e8294b2d5ff30`.
- `src/migo_tower_native.cpp`: `9e0e49a0f7daff7586552558a5bfe929da8b4702adab7052ced1f5314cdfecd8`.
- `src/migo_tower_ai.cpp`: `a43e0904d427cf7739b97a0329a1dc9caa25389bb2c23b05bbc36e40eca79550`.
- `src/savegame.cpp`: `33d14f1b2ca3247dee0e52def2fedb3d397abd52c62fd3257bc26c2078c1226b`.
- `src/savegame_json.cpp`: `628eabec227d267103e57fa86aaf414629af5fc3dcba5e654aa86a9565f655a5`.
- `tests/bandit_live_world_test.cpp`: `8d54b5259143e901a3753ea3d111803b07e8b04474d5580ec060d6c5b4386bd3`.
- `tests/test_main.cpp`: `b6f402444b0b117d9964dcb8b93fefb059bba5cd408543f2cc06e945a1969303`.
- `tests/migo_population_native_test.cpp`: `b85fc2b066146a82402da2ec2135a42f041eddb105bc3106982ffb241cb56ea0`.
- `tests/migo_population_test.cpp`: `595ff7c49ab5e7deef9b72c3609fc8927d1ba8a69bcc59f4d7b0bb0b416fafa5`.
- `tests/migo_tower_test.cpp`: `33017958ddeea68ddbf5b987127037a920077b91a1a01638440254d2731ba90e`.
- `tests/overmap_party_travel_test.cpp`: `81d04dd57597acf2c8f3aa1fb5805922a995d159e58bd8b01bd08d971d1552ed`.
- `tools/openclaw_harness/startup_harness.py`: `cd97fca0fbb6ef936152b6076e9e6641fbbbabc68a530225f997b24961e60013`.
- `tools/openclaw_harness/scenario_registry.py`: `83ba24296110a3a8f601e49349a8e20ad378514e7bfb44af019be0a4f3dc7ffc`.
- `tools/openclaw_harness/writable_run_owner.py`: `b5a0f7c35f631f193f48590201c7c851f74d9aac28475e511ff1bf943050c275`.
- `doc/overmap-ai-field-guide.md`: `a01517a842b6a50d89a60e7c11d61504149b0be5548b80c4a8f38f2018910c3d`.

- `src/npcmove.cpp`: `5a540568b7910463ff028dde2ea2a588e1c1a5cd2308e282d61e037ec025cd7a`.
- `src/monmove.cpp`: `67044d3d7d796e279a487780fdaea4d4389ad123cbd9ebb80096026e499f9274`.
- `src/physical_navigation.h`: `aaa10e273d743f70da0d2b8b31df580091d9f5cc1e745a73b2ed6f6c98c5a763`.

- `tools/openclaw_harness/play_cli.py`: `ef8ba3372c66f2e76a1ae402341dc2a201e93810d605c7cb0faa5c04cdf29043`.
- `tools/openclaw_harness/world_cli.py`: `b1b1344d5bdbf4828fe6be7ee02384642341282432ce4904d812a0453d9be061`.
- `tools/openclaw_harness/world_inspect.py`: `55c23e6bf22e8fcfcbb547e88255dcc17e598e79fe8d1ce0ad16d61a9c005f4d`.
- `tools/openclaw_harness/world_prepare.py`: `d1fc2333fef914d69ff4459cbcb055833aca8d56767bf9c01191e07b2274b794`.
- `tools/openclaw_harness/world_scratch.py`: `543159a64a321c697a2ffed5c6bb5c2fcca15c08c583368f377b5f390773f78c`.
- `tools/openclaw_harness/world_examples.py`: `798d52d5d301744570ddeb35d2f5738b228d48f61169b851a716872099be29f7`.
- `data/json/effects_on_condition/example_eocs.json`: `876d4f2c041c3ea157439c80c2ec38f592bf093241665222c4d0f939f4c384bc`.
- `tests/debug_prevent_death_test.cpp`: `11e79a1ec4ec4d8b9292560ab6aa59ae059a5c282e8174591b665ce91ae6111a`.

## Historical acceptance and compatibility identities

These entries preserve the previous stable claim/slice names and recorded receipt lines. Their
full original claim/proof scope is in the verified FS archive named above, located by the identical
slice marker. Checked entries are historical scoped status, not a claim of new testing. Previously
unproved and withdrawn entries are not newly commissioned work. This appendix neither reopens
accepted work nor grants fresh campaign obligations historical credit. The consumed work ledger
was `# Current work ledger` with no remaining checked rows at inspection; durable acceptance stays
in the existing database, unchanged by FS authoring. It contains 122 acceptance rows, including
two invalidated rows, at inspection. The database retains precedence over any historical checked
marker; this appendix does not reinstate an invalidated acceptance.

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-HOMEWARD-CONTINUITY-S001 claim=R-CAOL-HOMEWARD-CONTINUITY -->
### Genuine scouting and return through moving reality bubbles
- [x] 🟢 R-CAOL-HOMEWARD-CONTINUITY — A returning bandit dispatch continues off-bubble with the same surviving identities, materializes legally and completes only at its camp.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-HOMEWARD-CONTINUITY-closure-verify-001` (durable acceptance #1, 1792475800.5053701). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-HOMEWARD-CONTINUITY-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-HOMEWARD-CONTINUITY-S001 claim=R-CAOL-HOMEWARD-CONTINUITY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-CAMP-OBSERVERS-S001 claim=R-CAOL-CAMP-OBSERVERS -->
### Staffed camp and active dispatch observe independently
- [x] 🟢 R-CAOL-CAMP-OBSERVERS — Every valid living home roster observes while abstract or loaded, independently of an active dispatch, with truthful exclusion reasons.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-CAMP-OBSERVERS-closure-verify-001` (durable acceptance #1, 1792560737.027324). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-CAMP-OBSERVERS-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-CAMP-OBSERVERS-S001 claim=R-CAOL-CAMP-OBSERVERS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-FIRST-SMOKE-S001 claim=R-CAOL-FIRST-SMOKE -->
### Smoke can be the first admitted cannibal signal
- [x] 🟢 R-CAOL-FIRST-SMOKE — A valid smoke-first packet reaches cannibal response admission and can materialize the needed members and dispatch without prior light.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-FIRST-SMOKE-closure-verify-001` (durable acceptance #1, 1794239476.434369). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.
### Recoverable startup diagnostics must not strand playtesting
### Current repair: committed cannibal raid must assault despite being seen

Original scope: verified FS archive, slice `R-CAOL-FIRST-SMOKE-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-FIRST-SMOKE-S001 claim=R-CAOL-FIRST-SMOKE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-PATROL-SOUND-S001 claim=R-CAOL-PATROL-SOUND -->
### Patrol duty owns a temporary visible sound setting
- [x] 🟢 R-CAOL-PATROL-SOUND — Active roaming and fixed-post patrol show Investigate sounds ON, act consistently and restore each NPC's own prior setting on every duty exit and save/load.
  - Delivery status: Accepted in durable claim acceptance #1 at 1793748539.92382 through `R-CAOL-PATROL-SOUND-closure-verify-001`. Current-source caller tests and native loop/hold, personal-setting, duty-exit and reload evidence are retained in receipts `9340b636` and `cc5f9dad`. The controlled sound/movement comparison has bounded scope; no independent wall/LOS/internal-target, full-suite or Windows proof is claimed. This corrects the stale delivery label without changing the behavior below or reopening accepted work.

Original scope: verified FS archive, slice `R-CAOL-PATROL-SOUND-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-PATROL-SOUND-S001 claim=R-CAOL-PATROL-SOUND -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-DISPATCH-VISIT-S001 claim=R-CAOL-DISPATCH-VISIT -->
### Search, individual combat choice and bounded departure
- [x] 🟢 R-CAOL-DISPATCH-VISIT — Visiting bandit/cannibal dispatches search the target area, respond to valid enemies and depart through real local/abstract travel without losing individual survival choices.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-DISPATCH-VISIT-bound-evidence-verify-002` (durable acceptance #1, 1793842204.158325). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-DISPATCH-VISIT-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-DISPATCH-VISIT-S001 claim=R-CAOL-DISPATCH-VISIT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-STALKER-COUNTERATTACK-S001 claim=R-CAOL-STALKER-COUNTERATTACK -->
### A stalker reconsiders a nearby attacker
- [x] 🟢 R-CAOL-STALKER-COUNTERATTACK — Repeated reachable human attacks cause prompt, explainable stalker target/exposure reconsideration and a strike, cover move or withdrawal rather than unexplained stationary holding.

Original scope: verified FS archive, slice `R-CAOL-STALKER-COUNTERATTACK-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-STALKER-COUNTERATTACK-S001 claim=R-CAOL-STALKER-COUNTERATTACK -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-CAMP-WAIT-COST-S001 claim=R-CAOL-CAMP-WAIT-COST -->
### Responsive camp waiting with active NPCs
- [x] 🟢 R-CAOL-CAMP-WAIT-COST — Waiting and sleeping with two assigned NPCs stays responsive and scales without making work, patrol, interaction or danger response inert.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-CAMP-WAIT-COST-closure-verify-001` (durable acceptance #1, 1794482000.474596). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-CAMP-WAIT-COST-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-CAMP-WAIT-COST-S001 claim=R-CAOL-CAMP-WAIT-COST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-LIGHT-EMISSION-S001 claim=R-ZL-LIGHT-EMISSION -->
### Working emission
- [x] R-ZL-LIGHT-EMISSION — Carried and stationary light use actual emission facts without borrowed exposure or phantom power.

Original scope: verified FS archive, slice `R-ZL-LIGHT-EMISSION-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-LIGHT-EMISSION-S001 claim=R-ZL-LIGHT-EMISSION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-LIGHT-OPTICS-S001 claim=R-ZL-LIGHT-OPTICS -->
### Physical optical eligibility
- [x] R-ZL-LIGHT-OPTICS — Physical light reaches each observer through geometry independently of terrain recognition.

Original scope: verified FS archive, slice `R-ZL-LIGHT-OPTICS-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-LIGHT-OPTICS-S001 claim=R-ZL-LIGHT-OPTICS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-LIGHT-CONTINUITY-S001 claim=R-ZL-LIGHT-CONTINUITY -->
### Finite physical-light consequences
- [x] R-ZL-LIGHT-CONTINUITY — Brief genuine exposure can create a finite remembered investigation without immortal refresh or sound bypass.

Original scope: verified FS archive, slice `R-ZL-LIGHT-CONTINUITY-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-LIGHT-CONTINUITY-S001 claim=R-ZL-LIGHT-CONTINUITY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-STALKER-S001 claim=R-ZL-STALKER -->
### Stalker opportunity and commitment
- [x] R-ZL-STALKER — The stalker exploits a credible opening, reaches contact and counts attacks instead of approach decisions.

Original scope: verified FS archive, slice `R-ZL-STALKER-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-STALKER-S001 claim=R-ZL-STALKER -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-RIDER-PURSUIT-S001 claim=R-ZL-RIDER-PURSUIT -->
### Rider pursuit
- [x] R-ZL-RIDER-PURSUIT — A rider maintains physical pressure instead of routine post-shot retreat or failed-annulus withdrawal.

Original scope: verified FS archive, slice `R-ZL-RIDER-PURSUIT-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-RIDER-PURSUIT-S001 claim=R-ZL-RIDER-PURSUIT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-RIDER-IMPACT-S001 claim=R-ZL-RIDER-IMPACT -->
### Rider contact
- [x] R-ZL-RIDER-IMPACT — A physical approach produces a paid, mitigated contact attack with recovery.

Original scope: verified FS archive, slice `R-ZL-RIDER-IMPACT-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-RIDER-IMPACT-S001 claim=R-ZL-RIDER-IMPACT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-PREDATOR-LIFECYCLE-S001 claim=R-ZL-PREDATOR-LIFECYCLE -->
### Durable predator actors
- [x] R-ZL-PREDATOR-LIFECYCLE — One predator identity continues between local and abstract owners.

Original scope: verified FS archive, slice `R-ZL-PREDATOR-LIFECYCLE-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-PREDATOR-LIFECYCLE-S001 claim=R-ZL-PREDATOR-LIFECYCLE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-EVOLUTION-S001 claim=R-ZL-EVOLUTION -->
### Evolution and ammunition
- [x] R-ZL-EVOLUTION — Normal new-world evolution respects the configured season gate and initializes rider ammunition once.

Original scope: verified FS archive, slice `R-ZL-EVOLUTION-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-EVOLUTION-S001 claim=R-ZL-EVOLUTION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-RIDER-BANDS-S001 claim=R-ZL-RIDER-BANDS -->
### Persistent physical rider bands
- [x] R-ZL-RIDER-BANDS — Credible encounters form persistent bands and share only observed evidence.

Original scope: verified FS archive, slice `R-ZL-RIDER-BANDS-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-RIDER-BANDS-S001 claim=R-ZL-RIDER-BANDS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-ENCOUNTERS-S001 claim=R-ZL-ENCOUNTERS -->
### Encounter and description evidence
- [x] R-ZL-ENCOUNTERS — Early stalker and late rider availability and descriptions have their accepted natural-route evidence.

Original scope: verified FS archive, slice `R-ZL-ENCOUNTERS-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-ENCOUNTERS-S001 claim=R-ZL-ENCOUNTERS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-ASTRA-WORKER-S001 claim=R-MAINT-ASTRA-WORKER -->
### Existing optional worker capability
- [x] R-MAINT-ASTRA-WORKER — The previously accepted optional worker/dashboard capability retains its original scope.

Original scope: verified FS archive, slice `R-MAINT-ASTRA-WORKER-S001`.
<!-- DE67:DFS-SLICE:END id=R-MAINT-ASTRA-WORKER-S001 claim=R-MAINT-ASTRA-WORKER -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-CREATURE-SPRITES-S001 claim=R-ZL-CREATURE-SPRITES -->
### Separate creature sprites
- [x] R-ZL-CREATURE-SPRITES — The separately packaged stalker/rider sprites retain their accepted native visual integration.

Original scope: verified FS archive, slice `R-ZL-CREATURE-SPRITES-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-CREATURE-SPRITES-S001 claim=R-ZL-CREATURE-SPRITES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-LIGHT-FIXTURE-S001 claim=R-CAOL-LIGHT-FIXTURE -->
### Player-tile discovery and reliable restoration
- [x] R-CAOL-LIGHT-FIXTURE — The real loaded-source index retains stationary light beneath the player and touched fixtures restore global definitions on every exit.

Original scope: verified FS archive, slice `R-CAOL-LIGHT-FIXTURE-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-LIGHT-FIXTURE-S001 claim=R-CAOL-LIGHT-FIXTURE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-HANDOFF-S001 claim=R-CAOL-HANDOFF -->
### Repeated real ownership transitions
- [x] R-CAOL-HANDOFF — Repeated local/abstract crossings and save/load preserve one owner, identity and resource accounting, including rejected and interleaved transfers.

Original scope: verified FS archive, slice `R-CAOL-HANDOFF-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-HANDOFF-S001 claim=R-CAOL-HANDOFF -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-LIGHT-TURNS-S001 claim=R-CAOL-LIGHT-TURNS -->
### Turn-driven observation, memory and ordering
- [x] R-CAOL-LIGHT-TURNS — Real advancing turns detect eligible brief exposure, retain finite memory after source-off and expire it without redraw/query refresh.

Original scope: verified FS archive, slice `R-CAOL-LIGHT-TURNS-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-LIGHT-TURNS-S001 claim=R-CAOL-LIGHT-TURNS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-HISTORY-S001 claim=R-CAOL-HISTORY -->
### Perception history and coordinate lifetimes
- [x] R-CAOL-HISTORY — Actor replacement, bubble shifts and world/load boundaries cannot turn obsolete perception into current pressure or rider knowledge.

Original scope: verified FS archive, slice `R-CAOL-HISTORY-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-HISTORY-S001 claim=R-CAOL-HISTORY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-LIGHT-EXTRACT-S001 claim=R-CAOL-LIGHT-EXTRACT -->
### Share discovery and extract cohesive turn responsibilities
- [x] R-CAOL-LIGHT-EXTRACT — Duplicated light discovery and cohesive turn implementation are simplified with executed behavior preserved.

Original scope: verified FS archive, slice `R-CAOL-LIGHT-EXTRACT-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-LIGHT-EXTRACT-S001 claim=R-CAOL-LIGHT-EXTRACT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-SETUP-LIFETIME-S001 claim=R-CAOL-SETUP-LIFETIME -->
### Harness setup has a run lifetime
- [x] R-CAOL-SETUP-LIFETIME — R022 item setup is removed from ordinary turn implementation without suppressing later runs or duplicating intervention effects.

Original scope: verified FS archive, slice `R-CAOL-SETUP-LIFETIME-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-SETUP-LIFETIME-S001 claim=R-CAOL-SETUP-LIFETIME -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-BANDIT-QUERIES-S001 claim=R-CAOL-BANDIT-QUERIES -->
### Narrow bandit eligibility queries
- [x] R-CAOL-BANDIT-QUERIES — Identical eligibility checks share named queries without changing operation-specific transfer behavior.

Original scope: verified FS archive, slice `R-CAOL-BANDIT-QUERIES-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-BANDIT-QUERIES-S001 claim=R-CAOL-BANDIT-QUERIES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-DIAGNOSTICS-S001 claim=R-CAOL-DIAGNOSTICS -->
### Diagnostics stay outside decisions
- [x] R-CAOL-DIAGNOSTICS — Lengthy movement diagnostics can be separated and timing labels describe the measured interval without affecting play.

Original scope: verified FS archive, slice `R-CAOL-DIAGNOSTICS-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-DIAGNOSTICS-S001 claim=R-CAOL-DIAGNOSTICS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-WAIT-S001 claim=R-EFF-WAIT -->
### Pending response waits
- [x] R-EFF-WAIT — Waiting for an already-submitted native request stays within one tool execution until useful progress, terminal failure or the actual task deadline.

Original scope: verified FS archive, slice `R-EFF-WAIT-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-WAIT-S001 claim=R-EFF-WAIT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-FRAME-S001 claim=R-EFF-FRAME -->
### Reuse the returned observation
- [x] R-EFF-FRAME — The next legitimate action reuses a valid action-result frame without a redundant look.

Original scope: verified FS archive, slice `R-EFF-FRAME-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-FRAME-S001 claim=R-EFF-FRAME -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-EVIDENCE-S001 claim=R-EFF-EVIDENCE -->
### Exact projected evidence first
- [x] R-EFF-EVIDENCE — Known run/request/actor/event questions use existing exact filters and selected fields before bulk retrieval.

Original scope: verified FS archive, slice `R-EFF-EVIDENCE-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-EVIDENCE-S001 claim=R-EFF-EVIDENCE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-SEMANTIC-S001 claim=R-EFF-SEMANTIC -->
### Jev only for useful semantic selection
- [x] R-EFF-SEMANTIC — Telescope helps distinguish competing explanations without model calls for exact identifiers or known flags.

Original scope: verified FS archive, slice `R-EFF-SEMANTIC-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-SEMANTIC-S001 claim=R-EFF-SEMANTIC -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-CANDIDATES-S001 claim=R-EFF-CANDIDATES -->
### Candidate coverage before provider selection
- [x] R-EFF-CANDIDATES — Telescope's harness pool removes equivalent rows before submission and retains relevant late observations and contradictions within honest scan limits.

Original scope: verified FS archive, slice `R-EFF-CANDIDATES-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-CANDIDATES-S001 claim=R-EFF-CANDIDATES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-CONTINUATION-S001 claim=R-EFF-CONTINUATION -->
### Continue from current bindings
- [x] R-EFF-CONTINUATION — A context reset resumes the unfinished question without reconstructing static guidance or replaying pending input.

Original scope: verified FS archive, slice `R-EFF-CONTINUATION-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-CONTINUATION-S001 claim=R-EFF-CONTINUATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-ZL-PLAYTEST-S001 claim=R-ZL-PLAYTEST -->
- [x] R-ZL-PLAYTEST — The remaining stalker follows the player into natural city pressure and chooses its own opportunistic attack after honestly recorded debug-only stalker setup.

Original scope: verified FS archive, slice `R-ZL-PLAYTEST-S001`.
<!-- DE67:DFS-SLICE:END id=R-ZL-PLAYTEST-S001 claim=R-ZL-PLAYTEST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EXP-PIT-FUNDS-S001 claim=R-EXP-PIT-FUNDS -->
### Pit Crew request admission and durable funding shutdown — deferred
- [x] R-EXP-PIT-FUNDS — Optional Jev integrations stop admitting paid requests after confirmed provider exhaustion or the configured local budget, across worker restart and package update.

Original scope: verified FS archive, slice `R-EXP-PIT-FUNDS-S001`.
<!-- DE67:DFS-SLICE:END id=R-EXP-PIT-FUNDS-S001 claim=R-EXP-PIT-FUNDS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EXP-PIT-NOTICES-S001 claim=R-EXP-PIT-NOTICES -->
### Pit Crew evidence notices and normal packaging — deferred
- R-EXP-PIT-NOTICES — **Withdrawn from delivery.** A normally packaged optional Pit Crew produces useful, bounded evidence advisories while ordinary coordination remains authoritative and operational.

Original scope: verified FS archive, slice `R-EXP-PIT-NOTICES-S001`.
<!-- DE67:DFS-SLICE:END id=R-EXP-PIT-NOTICES-S001 claim=R-EXP-PIT-NOTICES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EXP-REFLEX-S001 claim=R-EXP-REFLEX -->
### Jev Reflex Pilot — deferred C-AOL harness experiment
- R-EXP-REFLEX — **Withdrawn from delivery.** A small optional native-action pilot is compared honestly with the current reasoning player and a meaningful deterministic baseline.

Original scope: verified FS archive, slice `R-EXP-REFLEX-S001`.
<!-- DE67:DFS-SLICE:END id=R-EXP-REFLEX-S001 claim=R-EXP-REFLEX -->

<!-- DE67:DFS-SLICE:BEGIN id=R-EFF-REGISTRY-COST-S001 claim=R-EFF-REGISTRY-COST -->
- [x] R-EFF-REGISTRY-COST — Routine scenario selection and exact evidence retrieval use compact current data without repeatedly rebuilding or returning the registry's accumulated history.

Original scope: verified FS archive, slice `R-EFF-REGISTRY-COST-S001`.
<!-- DE67:DFS-SLICE:END id=R-EFF-REGISTRY-COST-S001 claim=R-EFF-REGISTRY-COST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-NATIVE-REGRESSION-S001 claim=R-CAOL-NATIVE-REGRESSION -->
- [x] 🟢 R-CAOL-NATIVE-REGRESSION — The combined cleanup build preserves the affected light, actor ownership/history, setup and diagnostic behavior through substantive native regression playtests using established routes.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-NATIVE-REGRESSION-closure-verify-001` (durable acceptance #1, 1793898400.369239). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-NATIVE-REGRESSION-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-NATIVE-REGRESSION-S001 claim=R-CAOL-NATIVE-REGRESSION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-EXECUTION-S001 claim=R-HARNESS-EXECUTION -->
### Actions finish at a native outcome

Original scope: verified FS archive, slice `R-HARNESS-EXECUTION-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-EXECUTION-S001 claim=R-HARNESS-EXECUTION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-SESSION-S001 claim=R-HARNESS-SESSION -->
### A compact response and recoverable session
- [x] R-HARNESS-SESSION — Every supported operation returns enough current truth for the next decision, and collection/resume is idempotent without reconstructing request machinery.

Original scope: verified FS archive, slice `R-HARNESS-SESSION-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-SESSION-S001 claim=R-HARNESS-SESSION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-EVIDENCE-S001 claim=R-HARNESS-EVIDENCE -->
### Evidence and summaries answer the selected question
- [x] R-HARNESS-EVIDENCE — Exact evidence queries, scenario summaries and witness indexes expose related native facts and uncertainty without unrelated history or hand-built citation machinery.

Original scope: verified FS archive, slice `R-HARNESS-EVIDENCE-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-EVIDENCE-S001 claim=R-HARNESS-EVIDENCE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-PREMISES-S001 claim=R-HARNESS-PREMISES -->
### Scenarios establish a real opportunity for the tested behavior
- [x] R-HARNESS-PREMISES — Scenario preparation exposes executable native prerequisites before costly play and separates setup, infrastructure and gameplay failures.

Original scope: verified FS archive, slice `R-HARNESS-PREMISES-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-PREMISES-S001 claim=R-HARNESS-PREMISES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-CONTINUATION-S001 claim=R-HARNESS-CONTINUATION -->
### Continue useful work without reconstructing its environment
- [x] R-HARNESS-CONTINUATION — A worker receives an executable current boundary, valid next session operation and reusable build/evidence facts rather than repeated setup archaeology.

Original scope: verified FS archive, slice `R-HARNESS-CONTINUATION-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-CONTINUATION-S001 claim=R-HARNESS-CONTINUATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-LIFECYCLE-S001 claim=R-HARNESS-LIFECYCLE -->
### Save, quit and reload have separate observable boundaries
- [x] R-HARNESS-LIFECYCLE — Save → quit → reload → continue preserves the intended world and history and reports real completion at each boundary.

Original scope: verified FS archive, slice `R-HARNESS-LIFECYCLE-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-LIFECYCLE-S001 claim=R-HARNESS-LIFECYCLE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-CONSOLIDATION-S001 claim=R-HARNESS-CONSOLIDATION -->
### Replacements remove their old machinery

Original scope: verified FS archive, slice `R-HARNESS-CONSOLIDATION-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-CONSOLIDATION-S001 claim=R-HARNESS-CONSOLIDATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-JOURNEY-S001 claim=R-HARNESS-JOURNEY -->
### Prove the supported journey — H-JOURNEY

Original scope: verified FS archive, slice `R-HARNESS-JOURNEY-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-JOURNEY-S001 claim=R-HARNESS-JOURNEY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-PERFORMANCE-S001 claim=R-HARNESS-PERFORMANCE -->
### Performance alarms during real operations
- [x] R-HARNESS-PERFORMANCE — The shared operation driver reports measured slow turns, spikes, sustained regression and stalled game-time progress without confusing input waits, loading, saving or transport delay with simulation failure.

Original scope: verified FS archive, slice `R-HARNESS-PERFORMANCE-S001`.
<!-- DE67:DFS-SLICE:END id=R-HARNESS-PERFORMANCE-S001 claim=R-HARNESS-PERFORMANCE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-SIGNAL-PROOF-S001 claim=R-CAOL-SIGNAL-PROOF -->
### Native proof of staggered signals and camp confirmation
- R-CAOL-SIGNAL-PROOF — **Withdrawn from delivery.** Source-bound native before/after journeys prove the direct signal/camp fixes and affordable populated workloads under the current cadence, beyond helper tests and empty-map collection.

Original scope: verified FS archive, slice `R-CAOL-SIGNAL-PROOF-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-SIGNAL-PROOF-S001 claim=R-CAOL-SIGNAL-PROOF -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-TRAVELLING-COST-S001 claim=R-CAOL-TRAVELLING-COST -->
### Following a travelling player without repeated movement work
- [x] R-CAOL-TRAVELLING-COST — Loaded bandits/cannibals following a travelling player avoid redundant movement and unchanged failed-route work while preserving responsive pursuit, attacks, survival and ownership.

Original scope: verified FS archive, slice `R-CAOL-TRAVELLING-COST-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-TRAVELLING-COST-S001 claim=R-CAOL-TRAVELLING-COST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-EXIT-COST-S001 claim=R-CAOL-EXIT-COST -->
### Separate scout exit selection cost
- [x] 🟢 R-CAOL-EXIT-COST — Pair exit selection avoids full loaded-level enumeration and candidate cross products while preserving reachable complete-pair and survivor return semantics.

Original scope: verified FS archive, slice `R-CAOL-EXIT-COST-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-EXIT-COST-S001 claim=R-CAOL-EXIT-COST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-CAMP-ASSIGNMENT-S001 claim=R-CAOL-CAMP-ASSIGNMENT -->
### Fresh camp assignment
- [x] 🟢 R-CAOL-CAMP-ASSIGNMENT — A fresh camp accepts an eligible NPC through the native assignment route and retains the actual assignment through save/reload.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-CAMP-ASSIGNMENT-closure-native-proof-binding-001` (durable acceptance #1, 1790211537.935405). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-CAMP-ASSIGNMENT-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-CAMP-ASSIGNMENT-S001 claim=R-CAOL-CAMP-ASSIGNMENT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-CAMP-LOCKER-S001 claim=R-CAOL-CAMP-LOCKER -->
### Actual Locker equipment transfer
- [x] 🟢 R-CAOL-CAMP-LOCKER — Assigned camp NPCs acquire and equip real Locker weapons, compatible ammunition and applicable gear natively, with accountable item movement and reload persistence.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-CAMP-LOCKER-closure-native-transfer-conservation-001` (durable acceptance #1, 1790221758.925648). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-CAMP-LOCKER-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-CAMP-LOCKER-S001 claim=R-CAOL-CAMP-LOCKER -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-CAMP-DEFENSE-S001 claim=R-CAOL-CAMP-DEFENSE -->
### Patrol, zombie alarm and peaceful/hostile shakedown
- [x] 🟢 R-CAOL-CAMP-DEFENSE — A real armed camp patrol mobilizes its appropriate guards and defeats a zombie, permits peaceful shakedown/looting without guard attack, and engages a substantive hostile bandit defense fight.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-CAMP-DEFENSE-closure-verify-001` (durable acceptance #1, 1793794994.269684). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-CAMP-DEFENSE-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-CAMP-DEFENSE-S001 claim=R-CAOL-CAMP-DEFENSE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-CAMP-PAY-RETURN-S001 claim=R-CAOL-CAMP-PAY-RETURN -->
### Pay from camp stock and observe return ecology
- [x] 🟢 R-CAOL-CAMP-PAY-RETURN — Native payment can use the prepared camp inventory zone and gold/spillover; after replenishment the real bandit ecology is observed through return or a causally explained non-return boundary with elapsed game time.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-CAMP-PAY-RETURN-closure-verify-001` (durable acceptance #1, 1793820510.391628). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-CAMP-PAY-RETURN-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-CAMP-PAY-RETURN-S001 claim=R-CAOL-CAMP-PAY-RETURN -->

<!-- DE67:DFS-SLICE:BEGIN id=R-LAUNCHER-GEMMA-S001 claim=R-LAUNCHER-GEMMA -->
### Download and retain both Gemma 4 choices
- [x] 🟢 R-LAUNCHER-GEMMA — Catapult-Dabubu offers, downloads and selects `gemma4:e2b` and `gemma4:e4b` alongside its existing local models, and sends the retained selection to the installed game's local runner.
  - Delivery status: Already accepted at its recorded scope through `R-LAUNCHER-GEMMA-closure-gemma-proof-binding-001` (durable acceptance #1, 1790201995.8261409). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-LAUNCHER-GEMMA-S001`.
<!-- DE67:DFS-SLICE:END id=R-LAUNCHER-GEMMA-S001 claim=R-LAUNCHER-GEMMA -->

<!-- DE67:DFS-SLICE:BEGIN id=R-LAUNCHER-OLLAMA-SETUP-S001 claim=R-LAUNCHER-OLLAMA-SETUP -->
### Install Ollama from an absent runtime through the launcher
- [x] 🟢 R-LAUNCHER-OLLAMA-SETUP — Launcher-led setup installs and starts a real local Ollama runtime on macOS, Windows and Linux, pulls the selected model, and reports partial failure/readiness truthfully.
  - Delivery status: Already accepted at its recorded scope through `R-LAUNCHER-OLLAMA-SETUP-closure-setup-proof-binding-001` (durable acceptance #1, 1790205390.922957). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-LAUNCHER-OLLAMA-SETUP-S001`.
<!-- DE67:DFS-SLICE:END id=R-LAUNCHER-OLLAMA-SETUP-S001 claim=R-LAUNCHER-OLLAMA-SETUP -->

<!-- DE67:DFS-SLICE:BEGIN id=R-LAUNCHER-CLEAN-INSTALL-S001 claim=R-LAUNCHER-CLEAN-INSTALL -->
### Prove the complete first-time installation on all three OSes
- R-LAUNCHER-CLEAN-INSTALL — **Withdrawn from delivery.** A downloaded Catapult-Dabubu package completes the full clean launcher → game → Ollama/models/API → custom graphics → playable game → quit/relaunch journey separately on macOS, Windows and Linux.

Original scope: verified FS archive, slice `R-LAUNCHER-CLEAN-INSTALL-S001`.
<!-- DE67:DFS-SLICE:END id=R-LAUNCHER-CLEAN-INSTALL-S001 claim=R-LAUNCHER-CLEAN-INSTALL -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-REFERENCE-CAMP-S001 claim=R-CAOL-REFERENCE-CAMP -->
### Prove the owner's camp and physical signal comparison
- [x] 🟢 R-CAOL-REFERENCE-CAMP — Disposable TestSetup00/Test00 copies prove real camp equipment/patrol prerequisites and the open/closed/rooftop light, smoke and natural bandit-attraction behavior.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-REFERENCE-CAMP-closure-verify-001` (durable acceptance #1, 1793781558.5860648). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-REFERENCE-CAMP-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-REFERENCE-CAMP-S001 claim=R-CAOL-REFERENCE-CAMP -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-BULK-ITEM-COST-S001 claim=R-CAOL-BULK-ITEM-COST -->
### Measured bulk-item performance repair
- [x] R-CAOL-BULK-ITEM-COST — Owner closed the bulk-item investigation on 2026-09-24. This is a scope withdrawal, not proof of a performance repair or lost items. Josef says the planks were likely in the player backpack or burned and does not want further investigation of this issue.

Original scope: verified FS archive, slice `R-CAOL-BULK-ITEM-COST-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-BULK-ITEM-COST-S001 claim=R-CAOL-BULK-ITEM-COST -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-SMARTZONE-NAMES-S001 claim=R-CAOL-SMARTZONE-NAMES -->
### Distinguish Smart Zone Manager's generated names
- [x] 🟢 R-CAOL-SMARTZONE-NAMES — Newly generated Smart Zone Manager helper zones display `Smartzone …`, distinct from genuine basecamp zones, without changing zone behavior or custom names.
  - Delivery status: Already accepted at its recorded scope through `R-CAOL-SMARTZONE-NAMES-closure-native-proof-binding-001` (durable acceptance #1, 1790254038.4403222). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-CAOL-SMARTZONE-NAMES-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-SMARTZONE-NAMES-S001 claim=R-CAOL-SMARTZONE-NAMES -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-WINDOWS-DISPATCH-CRASH-S001 claim=R-CAOL-WINDOWS-DISPATCH-CRASH -->
### Ordinary dispatch preserves valid optic-item iteration
- [x] R-CAOL-WINDOWS-DISPATCH-CRASH — The owner's ordinary wait-through-dispatch crash has a source-bound lifetime repair and regression proof, with inventory/ecology behavior preserved and the prior proof gap explained at the current Mac-host scope.

Original scope: verified FS archive, slice `R-CAOL-WINDOWS-DISPATCH-CRASH-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-WINDOWS-DISPATCH-CRASH-S001 claim=R-CAOL-WINDOWS-DISPATCH-CRASH -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-WORKER-LINT-S001 claim=R-MAINT-WORKER-LINT -->
### Applicable worker checks produce actionable diagnostics without routine mutation
- [x] R-MAINT-WORKER-LINT — Applicable worker checks produce actionable diagnostics without routine mutation.

Original scope: verified FS archive, slice `R-MAINT-WORKER-LINT-S001`.
<!-- DE67:DFS-SLICE:END id=R-MAINT-WORKER-LINT-S001 claim=R-MAINT-WORKER-LINT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-FEATURE-MAP-S001 claim=R-MAINT-FEATURE-MAP -->
### One canonical feature map and coherent existing DFS retrieval
- [x] R-MAINT-FEATURE-MAP — One canonical feature map and coherent existing DFS retrieval.

Original scope: verified FS archive, slice `R-MAINT-FEATURE-MAP-S001`.
<!-- DE67:DFS-SLICE:END id=R-MAINT-FEATURE-MAP-S001 claim=R-MAINT-FEATURE-MAP -->

<!-- DE67:DFS-SLICE:BEGIN id=R-MAINT-SCENARIO-CONSOLIDATION-S001 claim=R-MAINT-SCENARIO-CONSOLIDATION -->
### Fewer meaningful base scenarios and effective fresh-worker guidance
- R-MAINT-SCENARIO-CONSOLIDATION — **Withdrawn from delivery.** Fewer meaningful base scenarios and effective fresh-worker guidance.

Original scope: verified FS archive, slice `R-MAINT-SCENARIO-CONSOLIDATION-S001`.
<!-- DE67:DFS-SLICE:END id=R-MAINT-SCENARIO-CONSOLIDATION-S001 claim=R-MAINT-SCENARIO-CONSOLIDATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-STALKER-PURSUIT-S001 claim=R-REL-STALKER-PURSUIT -->
### Stalkers follow observed travel, search and later revisit
- [x] 🟢 R-REL-STALKER-PURSUIT — Grounded three-OMT watching and bounded recurring pursuit work through real movement, loss and persistence.
  - Delivery status: Already accepted at its recorded scope through `R-REL-STALKER-PURSUIT-closure-verify-001` (durable acceptance #1, 1792517784.8294961). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-STALKER-PURSUIT-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-STALKER-PURSUIT-S001 claim=R-REL-STALKER-PURSUIT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-MIGO-ASSAULT-S001 claim=R-REL-MIGO-ASSAULT -->
### Tower observers send real assault parties directly
- [x] 🟢 R-REL-MIGO-ASSAULT — A valid tower observation admits a capability/loss-sensitive assault, with physical travel and individual return intent.
  - Delivery status: Already accepted at its recorded scope through `R-REL-MIGO-ASSAULT-closure-verify-001` (durable acceptance #1, 1792586722.4568162). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-MIGO-ASSAULT-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-MIGO-ASSAULT-S001 claim=R-REL-MIGO-ASSAULT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-MIGO-POPULATION-S001 claim=R-REL-MIGO-POPULATION -->
### Mi-go population has one source of truth and slow destroyable production
- [x] 🟢 R-REL-MIGO-POPULATION — Real inhabitants, casualties and slow production reconcile once across lazy discovery and map loading.
  - Delivery status: Already accepted at its recorded scope through `R-REL-MIGO-POPULATION-closure-verify-001` (durable acceptance #1, 1792555593.318798). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-MIGO-POPULATION-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-MIGO-POPULATION-S001 claim=R-REL-MIGO-POPULATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-RAID-SWEEP-S001 claim=R-REL-RAID-SWEEP -->
### Mi-go assaults and cannibal raids finish a safe bounded sweep
- [x] 🟢 R-REL-RAID-SWEEP — Assault search crosses source-tile borders, handles real hazards and resolves finite work without false search or refusal loops.
  - Delivery status: Already accepted at its recorded scope through `R-REL-RAID-SWEEP-closure-verify-001` (durable acceptance #1, 1793543637.14912). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-RAID-SWEEP-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-RAID-SWEEP-S001 claim=R-REL-RAID-SWEEP -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-SIGNAL-RANGE-S001 claim=R-REL-SIGNAL-RANGE -->
### Loaded smoke and elevated light can reach30 and40 OMT
- [x] 🟢 R-REL-SIGNAL-RANGE — Source strength and physical visibility support smoke up to30 OMT and elevated light up to40 OMT.
  - Delivery status: Already accepted at its recorded scope through `R-REL-SIGNAL-RANGE-closure-consumption-001` (durable acceptance #1, 1793374117.514038). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-SIGNAL-RANGE-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-SIGNAL-RANGE-S001 claim=R-REL-SIGNAL-RANGE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-EXPEDITION-REACH-S001 claim=R-REL-EXPEDITION-REACH -->
### Long expeditions retain one useful lifecycle and honest clocks
- [x] 🟢 R-REL-EXPEDITION-REACH — Camps can undertake useful30–40-OMT trips through mixed terrain without starvation admission or premature missing/return expiry.
  - Delivery status: Already accepted at its recorded scope through `R-REL-EXPEDITION-REACH-closure-verify-001` (durable acceptance #1, 1793740623.0105212). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-EXPEDITION-REACH-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-EXPEDITION-REACH-S001 claim=R-REL-EXPEDITION-REACH -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-AI-ACTIVATION-S001 claim=R-REL-AI-ACTIVATION -->
### Camps and parties have independent40-OMT active time
- [x] 🟢 R-REL-AI-ACTIVATION — A bounded player-centered overmap AI bubble suspends inactive work and clocks without breaking an independently active party.

Original scope: verified FS archive, slice `R-REL-AI-ACTIVATION-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-AI-ACTIVATION-S001 claim=R-REL-AI-ACTIVATION -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-REPEAT-ROBBERY-S001 claim=R-REL-REPEAT-ROBBERY -->
### Bandits remember successful places and learn from costly visits
- [x] 🟢 R-REL-REPEAT-ROBBERY — A successful robbery can select the same exact place later without a new signal, while empty and costly failed visits have distinct outcomes.
  - Delivery status: Already accepted at its recorded scope through `R-REL-REPEAT-ROBBERY-closure-verify-001` (durable acceptance #1, 1793919822.7126021). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-REPEAT-ROBBERY-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-REPEAT-ROBBERY-S001 claim=R-REL-REPEAT-ROBBERY -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-MIXED-PERFORMANCE-S001 claim=R-REL-MIXED-PERFORMANCE -->
### Mixed-load capacity campaign — withdrawn from alpha
- R-REL-MIXED-PERFORMANCE — **Withdrawn from delivery.** Matched production workloads measure ordinary responsiveness, expanded-range costs and practical overload while preserving active behavior.

Original scope: verified FS archive, slice `R-REL-MIXED-PERFORMANCE-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-MIXED-PERFORMANCE-S001 claim=R-REL-MIXED-PERFORMANCE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-RELEASE-PLAYTESTS-S001 claim=R-REL-RELEASE-PLAYTESTS -->
### Compact release suite built from the original playtests
- [x] 🟢 R-REL-RELEASE-PLAYTESTS — The maintained suite covers every expansion mechanism and repeats a broad representative selection of existing gameplay through the real harness.
  - Delivery status: Already accepted at its recorded scope through `R-REL-RELEASE-PLAYTESTS-closure-verify-001` (durable acceptance #1, 1794170738.6527889). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-RELEASE-PLAYTESTS-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-RELEASE-PLAYTESTS-S001 claim=R-REL-RELEASE-PLAYTESTS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-REL-SLOW-TURN-ANALYTICS-S001 claim=R-REL-SLOW-TURN-ANALYTICS -->
### Lean automatic slow-turn analytics
- [x] 🟢 R-REL-SLOW-TURN-ANALYTICS — Automatically retain bounded, useful slow-turn receipts through the existing performance pipeline.
  - Delivery status: Already accepted at its recorded scope through `R-REL-SLOW-TURN-ANALYTICS-closure-verify-001` (durable acceptance #1, 1794175696.335226). Existing evidence limits and separate open findings remain. This is a checklist correction, not new proof or broader acceptance.

Original scope: verified FS archive, slice `R-REL-SLOW-TURN-ANALYTICS-S001`.
<!-- DE67:DFS-SLICE:END id=R-REL-SLOW-TURN-ANALYTICS-S001 claim=R-REL-SLOW-TURN-ANALYTICS -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-MONGROUP-LIGHT-S001 claim=R-CAOL-MONGROUP-LIGHT -->
### Conventional zombie group light attraction — withdrawn
Historical unproved scope, not new work: R-CAOL-MONGROUP-LIGHT — Existing conventional zombie mongroups, including groups not already flagged as hordes, can notice visible physical light and mobilize toward its source.

Original scope: verified FS archive, slice `R-CAOL-MONGROUP-LIGHT-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-MONGROUP-LIGHT-S001 claim=R-CAOL-MONGROUP-LIGHT -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-MIGO-CLAIRVOYANCE-S001 claim=R-CAOL-MIGO-CLAIRVOYANCE -->
### Clairvoyance ecology view includes Mi-go towers and their dispatches
- [x] R-CAOL-MIGO-CLAIRVOYANCE — With DEBUG_CLAIRVOYANCE enabled, the overmap ecology display reveals existing Mi-go towers and their actual dispatched parties, with useful inspection of their current state.

Original scope: verified FS archive, slice `R-CAOL-MIGO-CLAIRVOYANCE-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-MIGO-CLAIRVOYANCE-S001 claim=R-CAOL-MIGO-CLAIRVOYANCE -->

<!-- DE67:DFS-SLICE:BEGIN id=R-CAOL-SIGNAL-VISIBILITY-S001 claim=R-CAOL-SIGNAL-VISIBILITY -->
### Fire use has a credible day/night and weather concealment tradeoff
- [x] R-CAOL-SIGNAL-VISIBILITY — Distant observers discover daytime smoke or nighttime firelight only when current weather, exposure and geometry permit it.

Original scope: verified FS archive, slice `R-CAOL-SIGNAL-VISIBILITY-S001`.
<!-- DE67:DFS-SLICE:END id=R-CAOL-SIGNAL-VISIBILITY-S001 claim=R-CAOL-SIGNAL-VISIBILITY -->
