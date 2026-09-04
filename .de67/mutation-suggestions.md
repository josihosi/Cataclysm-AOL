# DE-67 mutation suggestion ledger

This is a consumable mutation queue, not history. User-authored entries carry explicit
mutation-scoped owner authority; reviewer candidates remain evidence-backed proposals. A successful
review removes only completed entries; a failed or unproved review removes nothing. Preserve a
blocked entry with its exact conflict, impossibility, missing authority, risk, or uncertainty.

The pending section is machine-owned protocol as well as owner input. Encode each owner batch as
one top-level `- Owner-authorized [trigger]: ...` or `- Owner-authorized [defer]: ...` entry, and
encode a lesser-authority reviewer proposal as `- [defer] Reviewer-authored proposal: ...`. Indent
every continuation. A trigger requests review at the next durable quiet junction; a defer does not.
Accumulate a batch under one deferred entry and promote that entry once when ready. Any other
top-level syntax retains legacy trigger behavior. Only the pending section uses a top-level hyphen;
consumed history uses an asterisk so a section-blind queue reader cannot reactivate old entries.

For a miss, keep immediate recovery distinct from the smallest repeatable method correction and
state the counterexample that could falsify it. For random or universal review, preserve the
applicable policy's stored target, scope, authority, and evidence limits; a guard result never proves
more than its inputs.

## Pending suggestions

No pending suggestions.

## Consumed suggestions

* Owner-authorized [trigger]: At the next durable quiet junction, force one cumulative review of the
  worker-facing evidence and continuity interface before dispatching more ordinary CAOL work. Do not
  interrupt a live worker, build, playtest, or native process merely because this entry exists. The
  problem is not missing proof rigor: comprehensive harness artifacts are being serialized into the
  model conversation, and successor workers are reconstructing current state by reading old briefs,
  registry dumps, build logs, reports, and broad search results. Preserve the full evidence on disk,
  the pragmatic LLM witness, exact identities, evidence classes, fail-closed behavior, current dirty
  frontier, accepted receipts, and `src/llm_intent.cpp` as a protected boundary. Change how workers
  receive and retrieve that evidence; do not weaken what acceptance means.

  Treat the following five entries as one forcing package with independently testable outcomes:

  1. **Artifact-backed quiet command output.** Make worker-facing registry, cockpit, scenario,
     observation, action, report-finalization, reconciliation, and build routes write their complete
     audit artifacts to durable files while returning only the compact operational receipt needed for
     the next decision: command outcome, current task/run/binding identities, material state delta,
     evidence class, first divergence or actionable failure, proof-journal entry IDs, artifact paths
     and digests, and useful narrow follow-up queries. Full candidate inventories, lifecycle history,
     transition streams, compiler invocations, and nested reports must not enter stdout merely because
     they were generated. Do not solve this with an arbitrary byte or line cap, silent truncation, or
     loss of evidence. A worker may explicitly retrieve a full artifact when a compact receipt is
     contradicted or genuinely insufficient.

  2. **Indexed pinpoint evidence retrieval.** Give durable witness and diagnostic records stable,
     compact, machine-searchable fields for claim, task, worker, run, scenario, binding, sequence,
     event type, evidence class, actor/action/receipt identity, status or verdict, first-divergence
     class, summary, and artifact reference where applicable. Provide narrow queries over those fields
     so a worker can ask for one current continuation, one actor receipt, one failure class, or the
     latest relevant journal entry without reading a whole report or grepping all of `build_logs/` and
     `.userdata/`. Compact JSONL suitable for `rg`/`jq` and an indexed projection may coexist; grep is
     a fallback, not the primary discovery protocol. Preserve a direct path from every compact result
     to the complete immutable artifact that supports it.

  3. **Outcome-sized worker brief and read plan.** Revise the generated worker-brief guidance and its
     behavioral contract so a dispatched worker receives the assigned outcome, current proof frontier,
     accepted or protected no-replay facts, first open boundary, exact current save/build/run/artifact
     bindings, relevant code or harness entrypoints, the smallest initial evidence reads that make the
     next decision possible, and the conditions that justify expanding the search. The brief should
     explain why each named read is relevant, not tell the worker to ingest the complete WEC, DFS,
     ledger, registry, historical reports, or repository by ritual. Preserve worker autonomy to inspect,
     repair, test, and change strategy; prohibit neither broad investigation nor full-artifact reads
     when current evidence actually requires them. The correction is progressive disclosure and exact
     footing, not a prescribed command sequence or a cosmetic shorter prompt.

  4. **Durable no-archaeology terminal and continuation receipt.** Require each worker result and
     execution-context handoff to settle the attempt with a compact durable receipt naming the achieved
     outcome or first divergence, material repository and runtime changes, journal entries, tests and
     live actions performed, evidence ceilings, exact continuation artifacts and bindings, active work
     inherited by a successor, accepted proof that must not be replayed, the first remaining boundary,
     and narrow queries that can recover additional detail. The same coordinator should project this
     receipt into a successor packet instead of asking a fresh worker to rediscover state from old
     briefs and logs. An ended worker context may abandon an attempt without abandoning the outcome,
     but it must not orphan a live build or force the successor to infer ownership and state from the
     filesystem.

  5. **Continuous efficiency and context-engineering responsibility.** Make operational and systemic
     efficiency a first-class concern of mutation review alongside correctness, integrity, and honest
     proof. Monitor token use and context shape before reading contents: use compact metadata about
     source, approximate size, repetition, freshness, and role to surface both isolated large outputs
     and small recurring baggage. Inspect only opportunities that matter in the observed situation,
     apply the deletion test to simplify, remove, or make material retrievable on demand, and validate
     on a realistic worker route that the same outcome and honest proof remain possible; metrics must
     not become quotas, mandatory scans, or incentives to hide failures or end useful work early.

     Avoid archaeology: when relevant history truly must be reconstructed, the reviewer may ask
     optional native Luna helpers to inspect bounded artifacts and return compact independent
     conclusions with exact references, without DE67 tasks, clocks, ledger entries, or any transfer of
     the reviewer's mutation and acceptance responsibility.

  The review must determine the smallest correct division between compiled Phase-3 policy, generated
  brief/result contracts, coordinator guidance, harness/registry interfaces, proof journal, tests, and
  ordinary downstream implementation. It may update and behaviorally prove the mutation-scoped policy,
  coordinator, generated-brief, result-contract, and retrieval-contract surfaces directly. It must
  project repository-owned harness implementation that properly belongs to an ordinary worker into the
  refrozen DFS and active work ledger with explicit acceptance, then release the gate so the successor
  coordinator can dispatch that work. Do not require the exclusive reviewer to wait for work that only
  a post-mutation ordinary worker is authorized to perform. Equally, do not mark the downstream harness
  outcome accepted merely because its method contract and task projection exist. Do not leave a
  prose-only rule pointing at commands or schemas that no planned ordinary task will create, and do not
  make the mutation reviewer conduct the CAOL playtests whose interface it is correcting.

  Validate behavior rather than wording. Demonstrate that complete evidence is still written and
  recoverable by digest; ordinary stdout contains the causal receipt rather than the full artifact;
  pinpoint queries recover the same exact evidence as the referenced artifact; generated briefs do not
  demand blanket startup reading and do name sufficient current footing; and a successor can continue
  a preserved build or playtest from the terminal receipt without replaying completed setup or scanning
  historical logs. Record model-visible output size, fresh/cached token use when directly observable,
  command count, and context growth as evidence, not as invented gates or quotas.

  Reproduction evidence for the review: deleting the repository `Agents.md` reduced the fresh
  coordinator's initial input from 25,886 to 22,662 tokens, but inspected worker startups remained near
  21,350 tokens. After that deletion, the coordinator plus three R-029 workers consumed about 1,023,023
  directly observable fresh tokens and 38.55 million cached input tokens in roughly the first 48
  minutes. Individual registry and historical-log commands emitted hundreds of thousands to about one
  million raw characters; model-visible tool results repeatedly carried roughly forty thousand
  characters; one successor accumulated about 654,000 response-item characters and 149,947 fresh
  tokens in about six minutes while rereading harness guidance, old briefs, logs, diffs, registry state,
  and build output. The workers were trying to preserve current-source binding, no-replay evidence, and
  the first divergence, so treat this as an interface and handoff failure rather than worker misconduct.

  Do not add evidence retention or garbage collection to this forcing package. Referenced artifacts for
  active tasks, accepted claims, and continuations need a separate reachability and retention design;
  no owner retention period or deletion policy has been chosen. Do not add arbitrary token, output,
  search, command, retry, or file limits. The exclusive mutation review is complete when its method
  changes and generated contracts are behaviorally proved, every repository-owned implementation item
  has an executable acceptance contract in the refrozen DFS and active ledger, and the successor
  coordinator can dispatch that work without weakening current CAOL evidence or replaying closed proof.
  The overall five-route efficiency outcome remains visibly open in the work ledger until ordinary
  workers implement and behaviorally prove the projected harness and retrieval work; mutation-gate
  release is neither acceptance of that downstream work nor a reason to keep ordinary workers blocked.

  Disposition (gate `e8b6511ab381`, lineage `semantic-surface-cockpit`, 2026-09-04): consumed. The
  immediate recovery preserves R-026 and R-029 as compact identity-bound continuation receipts with
  exact first-open boundaries, no-replay work, bindings, narrow queries, and digest-bound artifacts;
  neither gameplay claim is accepted or replayed. The cause was an interface mismatch, not worker
  misconduct: the default registry route serialized a 4,236,128-character/223-entry full history and
  continuation depended on 151 and 6 free-text checkpoints, so responsible workers had to reread bulk
  evidence to protect binding and proof truth.

  The repeatable method correction is guarded and behaviorally exercised: worker-owned terminal
  transitions require an exact validated receipt; receipt queries are indexed by continuation and
  evidence identities and return compact projections unless full retrieval is explicit; successor
  packets contain the outcome, current frontier, compact receipt, exact bindings/entrypoints, and a
  reasoned progressive read plan. Artifact digest drift and terminal-without-receipt controls fail,
  one action-ID query returns only its compact match, and a successor packet omits bulky accepted
  history while retaining the first open boundary. Metrics remain diagnostics rather than quotas.

  The complete five-route outcome remains red as `R-030`. Its active ledger item and frozen
  `R-030-S001` acceptance contract require ordinary workers to implement quiet artifact-backed CAOL
  command output, cross-surface pinpoint retrieval, CAOL skill/test alignment, and a lossless fresh
  successor proof before further expensive R-026/R-029 playtests. No retention/garbage-collection
  policy or arbitrary limit was introduced, and `src/llm_intent.cpp` was not changed.

* Owner-authorized [trigger]: Unearth the previously designed current-source CAOL feature playtest
  package from this ledger's consumed history and place it in conjunction with—not in replacement
  of—the active semantic-surface delivery. Preserve the accepted R-SURFACE evidence, allow the
  current R-SURFACE-009 and R-SURFACE-010 work to finish, and treat the complete semantic cockpit as
  enabling footing for richer playtests rather than as the final gameplay outcome. This suggestion
  must not interrupt the present coordinator merely because it exists.

  During the next ordinary mutation review, inspect the consumed entry beginning “Reshape de67 3
  delivery around a current-source CAOL feature playtest package,” its recorded disposition, the
  current refrozen `.de67/DFS.md`, `.de67/work-ledger.md`, accepted semantic-surface receipts, current
  source, fixtures, scenario registry, playtest findings, and any surviving package artifacts. Then
  expand and refreeze the DFS and rewrite/refill the active work ledger so the integrated package is
  once again an explicit downstream frontier. Do not merely paste the old prose back, erase the
  semantic work, reopen accepted semantic claims, or write product/harness implementation during
  mutation review. Reconcile the old package with what the now-capable cockpit can actually operate
  and observe, preserve honest prior evidence with its ceilings, retire stale assumptions, and give
  ordinary coordinators outcome-sized work that uses the semantic surface to build, repair, and run
  the package afterward.

  Restore the package's intended gameplay scope unless current repository evidence justifies a
  narrower or revised route: follower and ambient LLM NPC behavior and snapshots, basecamp request
  context, Smart Zone Manager, Locker and Patrol behavior, overmap light/smoke/significant-sound
  causation, bandit and cannibal discovery/stalking/contact/attack/return lifecycles, and flesh-raptor
  behavior, while retaining the owner's earlier exclusion of writhing stalkers and zombie riders
  pending another design pass. Prefer combined living-world scenarios, independent claim verdicts,
  causal fire/signal controls, current-source bindings, mechanical checks plus gameplay feel, and
  reusable findings over isolated paperwork. Keep the established-base fixture and the detailed
  scenario/tooling/evidence guidance from the consumed package where still true, and update it where
  the semantic cockpit has removed an old harness limitation.

  Disposition (gate `1cb2f53ad1fe`, lineage `semantic-surface-cockpit`, 2026-09-02): consumed. The
  immediate recovery preserves durable acceptance and exact receipt references for all ten
  R-SURFACE claims, composes R-026 through R-028 into the refrozen DFS, and rewrites the active
  projection around the current established-base package. No accepted semantic claim was reopened,
  and no product, harness, fixture, scenario, test, or unrelated documentation was changed during
  this review.

  Incident reconstruction: the semantic DFS declared that it replaced the hostile-ecology DFS and
  retained only cockpit requirements. The active ledger then defined necessity solely against that
  narrowed WEC/DFS and explicitly deleted hostile-ecology work. The actor followed the supplied
  contract and incentive correctly; the earliest preventable cause was a destructive refreeze
  boundary with no rule for preserving a compatible owner-authorized unfinished downstream outcome.
  Immediate recovery alone would have pasted old claims back and left the same transition able to
  erase them again.

  Repeatable method correction: the active DFS now states one compositional rule in place of the
  replacement rule—an enabling refreeze may compact accepted footing, but it does not become a
  delivery ceiling or erase a compatible owner-authorized outcome that remains unproved. The ledger
  projects accepted semantics compactly and retains the unfinished product frontier, ordinary
  repository recovery authority, independent verdicts, and the bootstrap/validation split. Current
  evidence also retires two stale assumptions: staffed-camp signal observation is implemented and
  needs independent validation rather than a new implementation bridge, and the accepted semantic
  surfaces must be tried before inventing package-specific NPC interaction plumbing.

  Reproduction and counterexample: before correction, the durable mutation gate resolved to
  `owner-suggestion:1cb2f53ad1fe`, while both `.de67/DFS.md` and `.de67/work-ledger.md` contained no
  R-026, R-027, or R-028 route despite their consumed owner-authorized design. The current-source
  registry simultaneously selected the isolated R-027 scenario but rejected an older hostile-camp
  fire route for current contradiction, exposing why a verbatim restoration would be false. After
  correction, all ten R-SURFACE delivery markers and durable acceptance references remain closed,
  the slice inventory adds exactly R-026-S001 through R-028-S001 as red package work, R-027 records
  implemented source plus its zero-credit bootstrap and stale executable binding, and the active
  ledger contains meaningful stable subtasks for every non-atomic package item. Removing any one of
  those new claims again leaves the owner package contract unmet; preserving only the cockpit
  reproduces the original failure.

* Owner-authorized [defer]: Make optional Terra-to-Luna delegation easy to recognize without adding
  new administration or forcing delegation. Keep Terra responsible for its assigned outcome, but
  briefly tell it to consider a Luna helper when bounded work can return independently, especially
  an isolated live playtest witness, focused test run, log analysis, source trace, screenshot
  inspection, or platform check. For a playtest, Terra supplies the compact charter and isolated run
  context; Luna operates the run and returns the smallest journal-cited witness result; Terra judges
  the evidence and owns any repair.

  Preserve the existing native-helper model: no helper deadline task, DFS/ledger/clock mutation, or
  further delegation. Do not redesign parallel Terra dispatch or prescribe when Terra must delegate.
  Add only the smallest routed guidance needed to make this option discoverable, and keep helpers
  away from overlapping source edits or shared mutable runtime state unless they have explicit
  exclusive ownership.

  Disposition (cycle 3, 2026-09-01): consumed. The existing generated worker brief already allowed
  Terra to use native Luna helpers, but described the option only as useful “when that helps.” The
  supplied information therefore hid the decision boundary: it did not identify independently
  returnable work, the compact playtest handoff/result, or the shared-state ownership condition.
  This was an instruction-surface defect, not a worker failure. Immediate recovery preserves
  exploration-007's honest abandonment, source-bound implementation/build evidence, and the active
  R-SURFACE-007 route for deterministic talker selection; no live worker or product result required
  alteration. The repeatable correction rewrites the existing generated helper clause in place. It
  now makes bounded independent examples recognizable, keeps delegation optional, gives playtest
  helpers the compact charter and isolated run context, returns the smallest journal-cited witness
  to Terra for judgment and repair, and excludes helper-owned DE67 state or overlapping mutable work
  without explicit exclusive ownership. It adds no helper task, gate, quota, or coordinator step.

  Counterexample and proof: before the correction, the exact generated Terra message contained none
  of the bounded-work, isolated-playtest, journal-cited-witness, or shared-state ownership cues. The
  corrected unbound-task message contains each cue while retaining `fork_turns="none"`, Terra's
  whole-outcome responsibility, worker-selected effort, native helper freedom, collection/stop, and
  the no-further-delegation boundary; it also contains no `must delegate` instruction. All 55 focused
  policy-kernel tests pass. The cycle guard accepted the stored `orchestrator-guidelines.md` lane as
  an exact no-op because it already projects failed strategies and honest result settlement, and
  accepted only the generated-brief implementation and its behavioral boundary test.

* Owner-authorized [defer]: Reshape de67 3 delivery around a current-source **CAOL feature playtest
  package** which will exercise the implemented systems together in a living world, instead of
  preserving a cabinet of isolated old demos. This is a non-forcing suggestion for the next ordinary
  mutation review. Do not interrupt current work merely because this entry exists.

  This is a planning and steering mutation, not a package-implementation mutation. The mutation
  reviewer must research the current repository, then change, expand, and refreeze `.de67/DFS.md` and
  rewrite/refill `.de67/work-ledger.md` so ordinary de67 3 coordinators and workers can build and run
  the package afterward. Apart from the required mutation-ledger disposition, the reviewer must not
  write product or harness code, create scenarios or fixtures, change tests or other documentation,
  build the game, or conduct the playtests itself. Its deliverable is the clear map de67 3 will use,
  not the runnable package. Before editing that map, compare CAOL `dev` with the configured
  `upstream/master`, inspect the actual implementation, existing fixtures, prior playtest reports,
  current red claims, and active harness capabilities. Preserve useful prior evidence with its
  original provenance, but do not mistake an old green report or a deterministic test for
  current-product live qualification.

  The package scope is the CAOL runtime surface that is absent from, or materially extends, upstream:
  follower-NPC free-text LLM speech and actions with the NPC/world snapshot; ambient non-follower LLM
  speech; named-NPC/background and basecamp-request context where it reaches live behavior; Smart Zone
  Manager; the Basecamp Locker and its real downtime service; the local Basecamp Patrol zone, shifts,
  alarms, restraint, and persistence; hostile-ecology observation; overmap-relevant light, smoke, and
  significant-sound evidence; the persistent bandit scout/contact/shakedown/return lifecycle; the
  distinct cannibal night-attack lifecycle; and flesh-raptor circling/skirmisher behavior. Explicitly
  exclude writhing stalkers and zombie riders from this package because the owner wants another design
  pass over them first. Fixtures used for acceptance must contain neither excluded actor, and mixed
  historical rider/stalker scenarios must not supply package credit. Release plumbing, platform matrix
  work, and broad balance passes are also outside this playtest package.

  Start from what is honestly known. R-005 round `73ddab45b9af474dbf1485d1fcc53248`
  is the strongest current natural bandit lifecycle baseline, but it does not prove fire causation,
  camp-zone interaction, or LLM behavior. Historical follower chat proved basic speech plus several
  parsed actions, while the ambient scenario produced no credible ambient response and needs a real
  current run. Historical Locker runs proved persistent zone creation and an actual equipment change;
  Patrol connected/disconnected runs exposed rosters but left visible runtime conduct yellow. The
  historical significant-sound matrix was strong but lacks current source/executable binding. Older
  smoke/light and cannibal reports provide useful footing, not a substitute for a current causal
  lifecycle. The active R-008 roof/indoor family is still the natural place to prove player-created
  fire, channel attribution, response, camp interaction, return, and persistence. Reuse or deliberately
  reopen those claims where a combined current run gives materially better evidence; do not erase the
  old receipts or rerun them by ritual.

  Make the rewritten explanations useful to the agents who will actually do the work. Each relevant
  DFS acceptance branch and ledger task should explain the intended player-visible outcome, the CAOL
  production mechanism being exercised, why the proposed scenario can cause it naturally, what is
  already proved and what remains uncertain, which existing saves/tools are likely reusable, what
  setup receives zero feature credit, which independent claims can survive a mixed result, and what
  evidence would honestly settle the outcome. State genuine ordering or dependencies and the explicit
  rider/stalker exclusion. Do not prescribe one key sequence, fixed wait, or implementation tactic:
  give the coordinator enough context to select goal-oriented workers and give those workers room to
  inspect, combine, repair, restart, or improve the route. Focus the plan on producing a playable,
  causally credible package and learning from it—not on satisfying scenario paperwork.

  Build the smallest coherent family of branchable scenarios needed to cover materially different
  behavior. Prefer several outcomes from one credible world state, while keeping independent claim
  verdicts so a broken locker does not invalidate a valid bandit dispatch. The family should include:

  - A living-base day/night route in which known followers, assigned camp residents, and at least one
    eligible ambient NPC coexist. Let the worker converse naturally, issue several goal-oriented
    follower requests, observe speech and real actions, provoke an ambient speech-only response, inspect
    the snapshot/context actually sent to the backend, exercise a basecamp request or board handoff,
    observe Locker service and reservations during downtime, observe Patrol shift/route behavior, and
    save/reload. Include backend timeout/error visibility as an inverse path, without turning the run
    into an API test suite. Do not credit prompt text alone when the claim is downstream NPC behavior.

  - A bandit pressure route branching from the same established base. Use a production-owned camp
    outside the reality bubble and a true in-world signal, then observe evidence acquisition, dispatch,
    approach, local stalking/contact, and a genuine shakedown. Branch or clone at the meaningful choice
    so Pay and Fight can both be judged without replaying irrelevant setup. Patrol NPCs may watch, warn,
    or ready themselves during a shakedown, but must not start shooting until the encounter becomes a
    real fight or other hostile-combat state. In the fight branch, followers, patrol, Locker-supplied
    equipment, ambient reactions, departure/return, aftermath, and reload persistence may all provide
    separate evidence from the same session.

  - A materially distinct cannibal route using a production-owned cannibal camp and true darkness.
    Exercise signal-to-investigation/attack, direct hostile policy rather than extortion, camp defense,
    follower and ambient response, Locker/Patrol continuity, survivor return, aftermath, and reload.
    Daylight rally/hold may serve as a negative control when it answers the policy claim; do not make a
    cannibal behave like a bandit merely to share a scenario template.

  - A signal-control route that can isolate causation rather than treating any arrival as fire proof.
    Compare quiet/no-signal state, player-deployed and player-lit roof fire at night, and a matched
    ground-floor indoor pair with the relevant curtains/windows/openings genuinely closed versus open.
    Keep the owner-specified fire preparation available: ample spawned logs, a `SOURCE_FIREWOOD` zone
    across fuel-bearing tiles, normally deployed brazier, normally activated charged lighter, then the
    player two ordinary tiles away. Distinguish light, smoke, significant sound, scent, prior knowledge,
    and an already-dispatched party. Add a normal-player significant-sound stimulus such as audited
    gunfire, alarm, or explosion so the current sound path can be tested live; ordinary noise should be
    available as a negative comparison where the source contract predicts no significant event.

  - A flesh-raptor perimeter encounter in a base or field state where its circling, spacing,
    opportunistic attack, and response to multiple defenders can be watched alongside follower or
    Patrol behavior. Spawning the raptor is acceptable zero-credit setup when no natural lifecycle is
    being claimed; the observed production combat behavior, not the spawn, is the subject.

  Prefer one canonical established-base fixture with cheap derived branches over unrelated saves that
  silently disagree about factions, NPCs, zones, backend profiles, or hostile sites. Audit
  `bandit_basecamp_prepared_base_v1_2026-04-22` with the matching
  `mcwilliams_live_debug_2026-04-07` profile first. The saved state already contains the real
  `Bugchaser central` camp and bulletin position, player camp registration, `CAMP_FOOD`,
  `CAMP_LOCKER`, `CAMP_STORAGE`, two `CAMP_PATROL` zones, sixteen overmap NPCs, and known follower camp
  residents Katharina and Robbie. Its `your_followers.fac_food_supply` is zero, but that is not a
  defect, blocker, or reason to reject the fixture: only specific camp actions currently consume
  calories. Keep the food state visible as context and provision calories as receipted zero-credit
  setup only if a selected scenario actually uses an action whose production contract consumes them.
  Do not create a food transform, demand a stocked replacement save, or gate the general base preflight
  on a nonzero number merely to make the camp look administratively complete. Derive or recapture a
  composite only where the package needs materially different state, such as verified camp
  membership/assignment, suitable locker stock, patrol priorities, one stable ambient NPC,
  uncontaminated signal state, usable roof/interior openings, or the required production-owned
  bandit/cannibal sites. Ask the owner to create a new base save only if repository inspection proves
  that the current footing cannot support a necessary scenario; food supply alone cannot establish
  that need. `basecamp_dev_manual_2026-04-02` remains an alternative source of useful state, not a
  mandatory stocked fallback.

  Reuse existing capability instead of rebuilding it: `debug_spawn_follower_npc`, saved NPC audit,
  camp-assignment dialogue and repair transforms, basecamp/NPC/item/furniture/field/site transforms,
  fixture/profile install and capture, scenario registry, cockpit movement/wait/fire controls, and the
  claim-scoped witness/debug-finding route. Prefer stable existing named NPC identities over adding a
  deterministic NPC generator unless the scenario cannot otherwise exist. Add only gaps that the
  package proves necessary: a base-readiness preflight which checks camp registration and bulletin,
  required zones, assigned actors, production hostile origins, excluded-actor absence, and food only
  when the selected route actually requires it; a normal-player significant-sound bridge; and any
  missing LLM-facing action/observation needed to select an NPC, speak/yell, see the response, and bind
  the prompt/snapshot/action result. Keep credentials out of fixtures and receipts. Restore or build
  the current executable before transform validation if `zzip` is absent; that is readiness, not a new
  product lane. These are downstream work-ledger targets for ordinary workers, not changes the mutation
  reviewer should implement.

  Treat fixture mutations, spawned resources, clairvoyance, debug observation, NPC repair, and hostile
  site seeding as receipted zero-credit setup. Credit only production behavior reached through ordinary
  game actions after setup. The worker is the pragmatic witness and owns tactics, ordering, waiting
  horizon, branch choice, repair, and restart decisions. A divergence is a finding and a reason to keep
  learning when the remaining world state is still useful, not an automatic end to the playthrough.
  One session may prove the same claim in more than one way and may prove several systems at once;
  stable actor/item/site identity and causal attribution matter more than paperwork. Tests should
  protect fixture invariants, truthful bindings, forbidden-actor absence, evidence ceilings, independent
  mixed-result claims, and save/reload continuity—not one exact key sequence, one arbitrary wait, or one
  prescribed competent strategy.

  Route defects discovered during these combined playtests through the existing
  `.de67/debug-findings.md` queue instead of letting the playtest worker silently absorb every surprise
  into opportunistic code repair. The worker should report the observed defect, bound evidence,
  affected claims, and any observations that remain valid; the coordinator deduplicates and records it.
  By default, continue gathering unaffected evidence and let the coordinator decide afterward whether
  to schedule a repair, revalidation, or clean rerun through the work ledger. An immediate narrow repair
  is still allowed when the finding genuinely blocks the selected outcome and retaining the current
  context is the smallest useful route, but record the finding first and keep it open until the repair
  has been rechecked. Do not fix unrelated findings merely because their code is nearby, and do not let
  the existence of a finding invalidate independent green observations. Keep reusable missing
  observation/action/setup interfaces in the existing capability-gap route, as the findings-list
  contract already requires. The mutation reviewer should encode this routing in the DFS and work
  ledger; it should not prepopulate the queue or repair any finding itself.

  After the read-only research, thaw and expand the DFS so it names the integrated package outcome,
  in-scope feature surface, exclusions, causal scenario families, fixture/tooling boundaries,
  claim-scoped evidence semantics, and honest final acceptance. Then refreeze it against the inspected
  source baseline. Rewrite the active work ledger around outcome-sized downstream work rather than one
  task per primitive: preparation/readiness where genuinely necessary, implementation of missing
  package capabilities, creation of the combined scenario family, live playtesting, claim-preserving
  finding intake and coordinator-scheduled repair/revalidation, and a short current-source package guide
  with named load/handoff entries and staged-versus-natural caveats.
  Retain the useful spirit of the May `caol-josef-playtest-save-pack-*` cards without inheriting their
  stale binaries or evidence ceilings. Merge, retire, or reorder contradictory microscopic ledger work
  so the coordinator can see what matters next; preserve unrelated valid work and existing receipts.

  The mutation review is complete when the refrozen DFS describes the right package and proof boundary,
  and the work ledger gives the next coordinator a clear, sufficiently explained, outcome-sized route
  for delegating its implementation and playtests. Do not hold the mutation open until the package is
  coded or green. Those are subsequent de67 3 worker outcomes. A gameplay bug later discovered by the
  package may remain open as a routed finding; missing or dishonest package setup may not receive final
  package credit.

  - Non-forcing suggestion: Rework hostile-camp signal discovery around a genuine ecology finding,
  then align the DFS and active work ledger rather than reflexively patching one scenario. Staffed
  bandit and cannibal camps should act as stationary scouts: when smoke, elevated nighttime light,
  or another supported signal is genuinely observable from a camp under production distance,
  terrain/LOS, elevation, weather, and observer-capability rules, that camp should record its own
  signal lead. Requiring an unrelated roaming scout to encounter the signal, return, and report it
  before the camp can know about something visible from its own position creates a circular and
  implausible discovery path. This must not become omniscience or injected knowledge; mechanical
  evidence must identify the real camp observer, channel, source location, visibility calculation,
  and resulting durable memory.

  Make observation periodic, bounded, and deduplicated. Do not scan every camp against every field
  every turn or emit a new notice every millisecond while one fire burns. Research the existing
  production scheduler and signal collection path, reuse its coarsest responsive cadence and spatial
  indexing where practical, evaluate only eligible staffed camps and nearby/relevant signals, and
  update one stable lead identity for the same source/channel rather than appending duplicates.
  Re-observation should refresh confidence, last-seen time, or strength only when materially useful;
  unchanged observations should be cheap and quiet. Add performance-sensitive behavioral tests for
  a persistent fire across many turns, multiple fires and camps, duplicate suppression, cadence,
  bounded work, save/reload identity, extinguishing, and a genuinely out-of-range or occluded signal.
  Derive any exact cadence or bound from existing production clocks and measured responsiveness,
  rather than inventing a test-owned number.

  Separate discovery, memory, and response. Once credibly observed, the location should remain known
  after the fire disappears; signal freshness may reduce confidence and urgency, but should not erase
  the remembered place or make later reconnaissance inexplicable. Normal camp drive should still
  decide whether and when to investigate. Source inspection of run
  `7f9260c775d64822a059e450f6128f0d` shows the brazier was lit at minute 8221 and the witness stopped
  at minute 8280 after drive 357 missed the normal threshold 500. The relevant deterministic
  force-due path implies roughly a twelve-hour observation horizon, which is plausibly good gameplay
  rather than a defect. Close or reframe `R008-BANDIT-DRIVE-THRESHOLD-001`: the one-hour negative was
  premature, while missing camp-side observation and durable location memory are the actual findings.
  Do not lower dispatch thresholds merely to satisfy the harness.

  Review and coherently update the DFS, active work ledger, coordinator playtest contract, worker
  harness guidance, witness/finding lifecycle, and enforcing tests. Playtests must inspect the
  production variables and clocks on which behavior depends, expose enough state for the worker to
  choose a meaningful horizon, and allow the coordinator and worker to refine, extend, repair, or
  redesign proof. Every gameplay claim needs both mechanical proof of the authentic causal path and
  a pragmatic feel test of whether timing, feedback, choices, and player experience are good.
  Preserve mixed outcomes, accept multiple valid action/proof strategies, and treat an early
  `not yet` as such unless the governing mechanics make the outcome unreachable or unreasonable.

  - Non-forcing suggestion: Add a hostile-ecology performance qualification suite beside the large
  functional playtest package. A feature that is causally correct but tanks frame rate when bandits
  enter sight or begin stalking is not accepted gameplay. Research the observed slowdown and reshape
  the DFS and active work ledger around both deterministic performance measurements and live rendered
  feel tests; do not bury performance as one checkbox inside the functional witness or optimize from
  speculation before capturing a reproducible workload.

  Reuse and extend the existing `tools/hostile_camp_benchmark.py` machinery and its A/B ordering,
  source/fixture bindings, update-latency summaries, scheduler counters, CPU/wall time, and RSS
  sampling rather than creating an unrelated benchmark format. Inspect its current matrix and child
  workloads to identify what it already covers and what the sighted-stalking incident does not. Add a
  small representative family spanning: quiet established-world baseline; staffed camps observing a
  persistent nearby fire without duplicate-event churn; abstract scouts and routine dispatch; bubble
  materialization and approach; bandits visible to the player while stalking/pathing; multiple
  simultaneous hostile groups/signals/camps; and an intentionally extreme but valid stress state.
  Make the important local workloads explicit and independently comparable: a bandit shakedown while
  negotiation/watch behavior is active; the transition from shakedown into a hostile fight; a direct
  bandit hostile-fight scenario with patrol/follower defenders where applicable; a bandit fire/burning
  scenario covering ignition, ongoing fire damage and reactions, path reconsideration, death or
  retreat, and aftermath cleanup; and a signal-driven fire approach before any actor is burning.
  Capture the transition boundaries as well as steady states so a one-time materialization, hostility,
  target-list, path-cache, or aftermath spike cannot hide inside an average. Include cannibal
  stalking/attack, hostile fight, and burning equivalents where its scheduler or local policy is
  materially distinct. Prefer shared bound fixtures and controlled variants so differences are
  attributable rather than comparing unrelated saves.

  Measure distributions of frame time/frame pacing during rendered live scenarios, simulation turn
  or update latency, relevant scheduler/LOS/pathfinding/event counts, CPU time, and resident memory.
  FPS alone can hide stalls, and wall time alone can hide which subsystem exploded. When a regression
  reproduces, capture a focused profiler trace or equivalent call-stack evidence before changing
  architecture. Keep diagnostic instrumentation bounded and outside the hot path when inactive.
  Preserve raw measurements and machine/build identity, and compare paired current/baseline variants
  where possible. Derive regression judgments from measured baselines, intended supported hardware,
  and visible gameplay impact; do not invent a universal FPS number, fixed entity cap, or benchmark
  threshold merely to make the suite green.

  Treat performance acceptance like other playtesting: require mechanical evidence that work remains
  bounded and no repeated scan/event/path request grows accidentally, plus a pragmatic feel test that
  ordinary stalking and camp activity remain smooth enough to play. Extreme scenarios should reveal
  scaling shape and failure modes without becoming the ordinary balance target. Tests should catch
  duplicate signal refresh floods, every-turn camp-by-signal scans, repeated LOS/path recomputation,
  local/abstract ownership ping-pong, unbounded lead/event growth, and cleanup or save/reload leaks,
  while allowing equivalent efficient implementations. Give coordinators and workers the metrics,
  profiler artifacts, and production context needed to choose the repair; do not prescribe one
  optimization or let the suite become another administrative prison.

  Disposition (2026-08-31): accepted and projected as refrozen DFS claims R-026 through R-028 and
  outcome-sized active ledger work. Immediate recovery preserves prior focused evidence while naming
  one canonical current-source package, authentic signal ownership, and paired performance proof.
  Repeatable method correction is causal and claim-scoped: branch from one audited fixture, separate
  setup from behavior, preserve mixed verdicts, route defects independently, and bind mechanical
  evidence to pragmatic feel rather than one scripted ceremony.

  Incident reconstruction: isolated historical demos persisted because the prior DFS and ledger were
  organized around individual proof primitives, not a current-source integrated product outcome.
  The counterexample is a combined run where one branch fails but independent living-base or combat
  observations remain attributable and valid; R-026 now requires those separate verdicts and forbids
  old-card or mixed excluded-actor promotion.

  Signal reconstruction: production required an active outing observer and returned report before a
  camp could gain structural signal knowledge, so a stationary staffed camp could not remember what
  it could itself perceive. The one-hour `7f9260c7…8f0d` result was then framed as a drive-threshold
  defect even though drive had not reached its normal policy horizon. R-027 separates observation,
  memory, and response. Its falsifying controls are occluded/out-of-range camps, persistent and
  extinguished signals, multiple camps/sources, duplicate growth, save/reload identity, and ordinary
  drive remaining below threshold without being called broken.

  Performance reconstruction: the existing A/B owner measures abstract latency, CPU, RSS, counters,
  and equivalence, while the reported risk appears at rendered local transitions. R-028 preserves
  those baselines and adds paired materialization, stalking, shakedown/fight, defender, burning, and
  aftermath windows. A transient spike hidden by an acceptable average or an extreme workload that
  scales poorly without harming ordinary play is the required counterexample to simplistic verdicts.

  Gate reconstruction: the installed queue reader starts after `## Pending suggestions` but does not
  stop at the next heading, so hyphen bullets in consumed history re-entered the active queue. Before
  this disposition it returned seven entries and four historical triggers, producing stale gate
  `7714d3b50242`. Immediate recovery consumed the real deferred batch. The repeatable ledger method
  now reserves top-level hyphens for pending entries and uses asterisks for consumed history, keeping
  that history readable but parser-inert without exceeding this batch's three-artifact mutation
  authority. The original counterexample now returns zero pending entries and no mutation gate.

## Previously consumed suggestions

* Owner-authorized [trigger]: Reshape the active R-008 playtest trajectory around a reusable,
  long-horizon roof-fire ecology experiment instead of continuing to prove brazier, fuel, ignition,
  waiting, and each hostile response as isolated administrative fragments. This is a forcing
  mutation: inspect and align the DFS, active work ledger, scenario registry/declarations, CAOL
  harness guidance, LLM-facing cockpit/TUI capabilities, witness expectations, and tests that
  currently steer this work. Preserve valid prior evidence, but rewrite or retire active tasks and
  scenarios that keep the coordinator decomposing this natural lifecycle into one-fact runs.

  The canonical setup is ordinary playtest preparation and earns no ecology credit by itself. Choose
  a safely generated building with a usable roof near relevant production-owned ecology; shelters
  are one option, not a mandatory map template. Prefer natural roof access, while allowing a
  receipted player-only teleport to a valid roof tile as a zero-credit setup fallback or useful
  comparison. Do not teleport the hostile actors. At night, spawn enough logs that fuel spills
  across several nearby roof tiles; place a `SOURCE_FIREWOOD` zone across one or more log-bearing
  tiles; spawn a brazier item and deploy it through the normal player action; spawn or equip a
  charged lighter and use it through the normal player action to create a genuinely burning brazier.
  After ignition, move the player exactly two ordinary map tiles away from the brazier before the
  observation wait begins. Record setup honestly without requiring the worker to repeatedly re-prove
  already established harness primitives.

  Keep the relevant camp or other production-owned origin outside the reality bubble. The fire and
  smoke on an elevated roof at night are the stimulus. Let several in-game days pass when needed so
  the worker can observe a complete natural lifecycle: an abstract actor or group detects the
  signal, dispatches or approaches, enters the reality bubble, stalks/investigates/acts locally,
  disengages, leaves the bubble, and returns to durable abstract ecology state. Include save/reload
  or unload/reload observation when it helps prove persistence. Do not impose a short fixed wait as
  the definition of failure; expose good time, log, minimap/overmap, actor-identity, bubble-boundary,
  smoke/light, and durable-state information, then trust the worker to decide whether to keep
  waiting, intervene diagnostically, change terrain, restart, or improve the harness. One live
  session may contain several observations and prove several related claims.

  Treat this as a scenario family and causal laboratory, not a single overfitted bandit script. Use
  appropriate naturally generated buildings, roofs, surrounding terrain, distance, weather, season,
  and hostile origins to exercise whichever DFS claims fit: bandits, cannibals, writhing stalkers,
  zombie riders, smoke and light perception, dispatch timing, stalking, local/abstract conversion,
  departure, return, and persistence. The worker owns scenario fit and may vary one or more of these
  conditions creatively. A single playthrough may support multiple claims when the same observed
  lifecycle genuinely bears on them; keep actor identity and causal attribution clear rather than
  demanding duplicate ceremonies. Manufactured actors or ecology-changing debug interventions may
  diagnose tools or mechanics but receive zero natural-ecology credit; final natural claims must be
  witnessed from production-owned actors responding without actor teleportation or behavior
  fabrication.

  Add a paired indoor ground-floor control for the same causal family. In a naturally generated
  building, place and light the brazier through the same player-owned setup, close every relevant
  curtain, window, and exterior opening, step the player two tiles away, and observe for a
  lifecycle-appropriate duration. This route tests whether opaque walls and closed curtains really
  prevent the indoor fire's light from becoming an exterior detection signal. Compare it with a
  matched positive route in the same or equivalent building with curtains/openings exposed, rather
  than interpreting an uneventful indoor wait in isolation. Observe and distinguish light, smoke,
  sound, scent, pre-existing knowledge, and incidental encounters so a dispatch caused by another
  channel does not falsely disprove light blocking. Preserve honest inconclusive outcomes when the
  comparison cannot isolate the signal. Vary window layout, curtain state, building footprint,
  terrain, weather, and hostile ecology when useful, but let the worker choose the smallest matched
  comparison that answers the current claim. This indoor control is an important first-class member
  of the scenario family, not a later optional polish item.

  The mutation reviewer's concrete job is to inspect the unresolved acceptance claims and active
  red lamps first, then design, create, and validate a compact set of fire-signal scenarios that can
  answer those claims. Rework `.de67/work-ledger.md` substantially so the active spokes name these
  outcome-sized experiments and the connected claims each can settle. Thaw, rewrite, and refreeze
  `.de67/DFS.md` where its current decomposition, scenario requirements, or acceptance language does
  not represent this route; do not merely append the idea to historical prose. Create or revise the
  scenario declarations, fixtures, charters, witness surfaces, and behavioral tests needed to make
  the selected set runnable. Validate that each setup can genuinely produce its stated signal and
  target ecology before returning it to ordinary workers. Remove, merge, or demote superseded active
  ledger fragments so the old one-primitive trajectory does not continue alongside the rewrite.

  Work out a scenario matrix from the tests actually still required, without manufacturing a
  Cartesian-product bureaucracy. It should cover the materially distinct causal branches needed for
  bandit stalking, cannibal stalking, hostile attacks or raids, approach/localization, and aftermath.
  Each arrival must have a true in-world trigger—such as exposed light, visible smoke, or another
  source-backed signal the production ecology really consumes—not a fixture that silently places the
  attackers at the camp or grants them unexplained target knowledge. Include matched blocked-signal
  controls where they answer a real claim. One strong run may validate several branches, while a
  materially different hostility policy, signal channel, occlusion condition, or lifecycle boundary
  deserves a separate variant. Inspect the production signal, dispatch, hostility, and camp-defense
  code while choosing the variants so every declared causal route is mechanically possible.

  Integrate real basecamp zone behavior into suitable variants rather than postponing it to isolated
  tests. Set up and exercise the production `Basecamp: Locker` (`CAMP_LOCKER`) and camp patrol
  (`CAMP_PATROL`) zones while the signal-driven ecology unfolds. Observe whether locker workers,
  reservations, equipment access, and ordinary camp activity remain coherent before, during, and
  after contact. Patrolling NPCs should recognize genuinely hostile stalking, cannibal attacks,
  bandit attacks, or raids and defend the camp appropriately. They should not shoot or otherwise
  escalate a bandit shakedown merely because the shakedown party is present: the expected behavior is
  alarm/watch/readiness until the encounter becomes a fight or another real hostile-combat state.
  Test that distinction in live scenarios as well as retaining focused deterministic coverage. The
  existing production contract in `tests/faction_camp_test.cpp`, including camp-local locker/patrol
  zones and `camp_patrol_alarm_watches_active_shakedown_contact_without_combat_escalation`, is an
  anchor to validate in gameplay rather than a substitute for the playtest.

  Allow previously green or closed claims to be deliberately reopened when the improved cockpit and
  these combined scenarios can provide stronger, more natural, or cross-feature evidence. Reopening
  does not erase the earlier proof or imply it was false: retain its provenance, state why the new
  route is materially better, and distinguish regression revalidation from a genuinely new claim.
  Favor combined playtests when one coherent world state can exercise several interacting features,
  but never promote one observation beyond what it actually supports.

  A divergence in one feature must not automatically abort the playthrough or discard evidence for
  the other features. For example, a broken locker-zone interaction discovered during a valid
  signal-driven bandit approach is a locker finding; it does not by itself cancel the bandit
  detection, dispatch, stalking, or patrol observations. Continue the session when its remaining
  state can still answer other claims honestly. Record which claims the divergence affects, which
  remain observable, and whether an immediate repair, later rerun, or separate clean comparison is
  needed. Abort or restart only when the world state can no longer answer the intended claims, the
  causal attribution is contaminated, or the worker judges a clean experiment more useful—not
  merely because the first bug appeared.

  Give the coordinator one lightweight durable place to collect product and harness defects found
  during these multi-feature runs. Reuse an existing active defect surface if repository inspection
  finds one with the required semantics; the append-only SQLite capability-gap history is only for
  reusable missing observation/action/setup interfaces and must not be overloaded with ordinary
  gameplay bugs. Otherwise create `.de67/debug-findings.md` as a concise coordinator-owned queue.
  Each finding needs only a stable identity, short observed defect, bound run/evidence reference,
  affected claims, explicitly unaffected observations when useful, current disposition, and the
  next repair or revalidation action. It must not become a second DFS, a proof checklist, or an
  automatic mutation trigger. Workers report findings in their ordinary result; the coordinator
  deduplicates, records, schedules fixes alongside the outcome-sized work, and closes a finding only
  after an appropriate repair and recheck. Tests should demonstrate that one mixed run can emit a
  locker defect, retain valid bandit evidence, continue into patrol observation, and route the defect
  for repair without terminating the entire scenario.

  Repository anchors to reconcile rather than blindly preserve include `.de67/DFS.md` under the
  current scenario setup/intervention contract; `.de67/WEC.md` under deterministic setup and
  observation; `TechnicalTome.md`'s corrected fire-knot/source-zone description;
  `tools/openclaw_harness/startup_harness.py`'s `source_firewood_zone_near_player` and map item/
  furniture transforms; `tests/firestarter_activity_test.cpp`'s production brazier + wood + lighter
  ignition path; `tools/openclaw_harness/CONTROL_LOOKUP.md`'s historical brazier deployment notes;
  the existing `bandit.roof_fire_horde_*`, `bandit.player_lit_fire_signal_wait_mcw`, source-zone,
  visible-brazier, and real-fire scenario declarations and their backing fixtures. In particular,
  include the bandit/cannibal signal, pursuit, hostile-operation, patrol, shakedown, and camp-locker
  production paths and their focused tests when deriving the scenario matrix. Review short
  bounded-wait and one-primitive gates that made sense while repairing the harness but
  now obstruct the whole-life-cycle experiment. Keep tests for truthful bindings, real ignition,
  receipts, evidence ceilings, and persistence; replace tests that constitutionalize one exact key
  sequence, terrain, short duration, or one-fact-per-run decomposition with behavioral tests that
  admit several competent strategies and scenario variants.

  The resulting active DFS and ledger should contain outcome-sized spokes: where applicable, one
  selected task should cover several connected subtasks such as setup, exposed-roof response,
  closed-curtain indoor control, signal-channel comparison, dispatch, bubble entry, local behavior,
  locker/patrol interaction, issue collection, exit/return, and persistence rather than opening a
  new task for each clerical checkpoint. Consume this trigger only when the coordinator receives a
  clear reusable fire-signal lifecycle goal, the worker has the controls and observations needed to
  pursue it for several in-game days, mixed outcomes preserve claim-specific evidence and route
  defects without aborting unrelated work, obsolete contradictory active entries are removed or
  rewritten, and the loop can resume without treating setup paperwork, the first divergence, or a
  short uneventful wait as terminal proof.

  - Disposition: Consumed by exclusive mutation review for gate `3a0356194db2`. The incident began
    before worker execution: R-008's active ledger and scenario contracts split one natural ecology
    lifecycle into exact-input setup gates and short, terminal observations, so coordinators were
    rewarded for accumulating local green facts instead of preserving a causal experiment. Immediate
    recovery replaced that active decomposition with one outcome-sized fire-signal laboratory and a
    four-member roof/indoor scenario family; the old short-wait declarations remain blocked only as
    provenance. The repeatable correction makes each scenario own preparation, causal boundaries,
    and evidence ceilings while the worker owns horizon, tactics, and diagnosis. A claim-scoped
    witness bundle now keeps unaffected proof alive, routes ordinary defects through
    `.de67/debug-findings.md`, and rejects overlapping affected/unaffected claim sets; capability-gap
    history remains reserved for missing reusable interfaces.
  - Counterexample and proof: the mixed-run witness tests prove a locker contradiction can coexist
    with valid bandit and patrol observations, and reject a finding that tries to affect and preserve
    the same claim. Fixture tests apply the transforms to a real source save and verify night roof
    placement, three fuel-bearing tiles, `SOURCE_FIREWOOD`, no injected fire or hostile actors, exact
    two-tile movement requirements, and a closed/open pair differing only at the openings. Registry
    rebuild and query select the new executable roof route. The focused firestarter test passes four
    assertions. The shakedown/patrol test first reproduced the original stale-state failure, then
    passed all twelve assertions after its fixture was corrected to construct the authoritative
    committed `active_hostile_operation` instead of the retired generic outing surface.
  - Preserved uncertainty: no fresh playtest has yet proved that production ecology completes the
    new multi-day family. Fire/smoke may redirect or inform an already dispatched structural sortie,
    but current production code does not justify crediting fire as the initial bandit dispatch cause
    without a separately observed event. The refrozen DFS, charter, and ledger preserve that red
    claim and require claim-specific causal evidence; this ordinary playtest uncertainty does not
    preserve the mutation gate because the requested executable route and evidence method now exist.

* Owner-authorized [trigger]: Loosen the active playtesting and worker instruction system so it
  supplies goals, truthful context, capable tools, and a few real integrity boundaries without
  prescribing the agent's tactics. Perform a structural alignment pass across the installed de67 3
  coordinator prompts and policy obligations, `.de67/orchestrator-guidelines.md`,
  `.de67/test-and-task-guidelines.md`, `.de67/playtest-witness-contract.md`,
  `.agents/skills/caol-harness/SKILL.md`, the LLM-facing cockpit/TUI contracts and tool output, active
  DFS/ledger/charters, and tests that enforce those routes. Delete, merge, soften, or move procedural
  instructions instead of appending a generic “be creative” paragraph beneath them.

  Treat the live repository and installed de67 route as the research corpus. Trace each instruction
  from its authored source through the generated coordinator/worker payload and into any machine
  contract or test before changing it. Distinguish active routed policy from historical DFS evidence;
  delete or move inactive remnants instead of making workers obey them. Do not use online guidance as
  a substitute for this repository-grounded alignment pass.

  Local evidence anchors for the current overconstraint include
  `.de67/test-and-task-guidelines.md:13-17` (continue only after a broad positive materiality proof and
  otherwise fail closed), `:25-34` (one narrow fact and named causal question after every result),
  `:42-60` (charter-fixed shortcuts/stops, unsafe-divergence stop, and a derived maximum before every
  gameplay action), and `:68-79` (sentinel-before-authority and evidence-class procedure). Inspect
  `.agents/skills/caol-harness/SKILL.md:12-38` for the mandatory exact mutation workflow and `:70-86`
  for the single-`next_action`, repair-one-declaration, repeat-query loop. Inspect
  `.de67/playtest-witness-contract.md:6-35` for fields or rejection categories that have become
  paperwork rather than intellectual scrutiny. Inspect `.de67/orchestrator-guidelines.md:118-123`,
  where first-divergence continuation is still gated by positive proof and uncertainty fails closed.
  Follow generated runtime policy through `.de67/phase3-policy.json` and
  `.de67/phase3-contracts.json`, then through `coordinator_prompt`, `coordinator_ledger_contract`, and
  the worker-brief construction in
  `/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/coordinator_supervisor.py` and
  `/Users/josefhorvath/.codex/skills/de67/de-67-3/scripts/policy_kernel.py`; verify the installed de67
  prompt/contract tests do not silently restore removed tactics. Inspect the actual tool boundary in
  `tools/openclaw_harness/cockpit.py` (`keep_watch`, `keep_watch_unsafe_condition`, raw wait, and
  `guarded_move_relative`) and its LLM-facing recipes in `cockpit_tui.py`. Review enforcing tests,
  especially `cockpit_keep_watch_test.py` exact recipe/action assertions,
  `cockpit_raw_wait_test.py`, `r023_relative_movement_test.py`,
  `proof_classification_unit_test.py` repeat-query assertions, `r005_corridor_observation_test.py`,
  fixture mutation-contract tests, and any test that freezes one action sequence rather than an
  integrity boundary. Preserve binding-, contradiction-, receipt-, and evidence-credit tests such as
  `playtest_witness_test.py`; they protect truth rather than dictate exploration.

  Preserve the small hard kernel: user data and real external safety; source/executable/run/identity
  binding where a claim depends on it; honest observations and tool receipts; visible contradictions;
  evidence ceilings and no false promotion; cleanup ownership; and truthful terminal state. Treat
  everything else as strategy owned by the coordinator or worker. A playtest worker may explore
  messily, inspect broadly when useful, improvise native action sequences, change tactics, use debug
  interventions with honest zero-credit labeling, accept fictional injury or death, repair tools and
  scenarios, rerun, or abandon an unproductive strategy without earning administrative punishment.
  A coordinator brief should state the desired outcome, known evidence, real constraints, and useful
  tools—not a guessed implementation plan. Intermediate exploration need not look like final proof;
  only the claim presented for acceptance must satisfy the relevant truth and evidence boundaries.

  Make persistence creative rather than repetitive. The worker should stick with the playtest outcome
  and actively use the available game, debug, cockpit, TUI, map, witness, and inspection tools, but it
  does not owe loyalty to the first setup or action sequence. When a run becomes uncertain, it may
  rethink the causal model, inspect source or game state, restart with a better setup, or improve the
  instrument itself. If the missing fact is not observable, the preferred repair may be clearer game
  logging, richer structured observations or deltas, better minimap/overmap/status information, or a
  new composable TUI action—not another paragraph of procedure. Trace those opportunities through
  `tools/openclaw_harness/cockpit.py` observation/journal surfaces and
  `tools/openclaw_harness/cockpit_tui.py` (`_local_map`, `_overmap`, `_commands`,
  `_receipt_drilldown`, and `render_state`). Give the agent enough legible state and capable controls
  to invent a route the coordinator did not predict. Require honesty about what a restarted or
  instrumented run proves, but do not make uncertainty itself terminal or require advance approval
  to repair the harness. Favor inspired problem-solving and useful new observability over repeating
  an uninformative run or polishing its paperwork.

  Do not equate one playthrough with one proof attempt. The same claim may be demonstrated through
  different causal routes and may be observed more than once during a single live session. Let the
  worker retain useful world state, recover from an unhelpful event, reposition actors, wait again,
  try another native stimulus, or collect a second independent observation without automatically
  finishing or restarting the run. A divergence normally becomes context for the next action in that
  playthrough, not a reason to abandon it. Restart only when the current world state can no longer
  answer the claim honestly or when the worker judges a clean setup to be the better experiment.
  Receipts and witness output should be able to represent multiple attempts and observations while
  keeping their causal routes and evidence ceilings distinguishable.

  Explicitly remove `stop at the first divergence` as a worker or playtest policy. Preserve the first
  divergence as diagnostic evidence, but require a real effort to make the assigned proof work:
  inspect it, act, repair, change tactics, use available TUI/debug capabilities, rerun when useful,
  and continue inside the same outcome-sized task. A divergence is neither success nor an automatic
  terminal finding. Stop only when the outcome is proved, the agent concludes from evidence that the
  strategy or claim is genuinely contradicted, or continuation needs a real external/human decision,
  irreversible user-data risk, unavailable system capability, or materially different owner outcome.
  Delete or rewrite charter stop conditions, ledger phrasing, coordinator briefs, harness defaults,
  and tests that terminalize a run or worker merely because it encountered the first unexpected game
  event. Do not replace this with a fixed retry count or another administrative continuation test;
  trust the agent to judge whether continued effort advances the proof.

  Replace process-shaped tests with behavioral tests that admit multiple competent strategies and
  witness wording. Keep exact tests only for an actual machine protocol, irreversible side effect,
  identity/binding invariant, contradiction, or credit boundary. Simulate messy playtests where the
  agent continues after fictional danger, changes approach, uses an optional tool creatively, skips
  irrelevant clerical work, and still returns an honest supported or inconclusive result. Include
  inverse cases where a false binding, hidden contradiction, fabricated observation, or promoted
  zero-credit intervention is rejected. Compare prompt/tool payloads before and after: the revised
  route should provide less tactical control while retaining or improving outcome truth. Consume this
  trigger only after DFS, ledger, skills, prompts, TUI/tool surfaces, and enforcing tests agree on the
  same “structure, not prison” model.

  Disposition — resolved by exclusive review `b95c62281c33`. The incident was produced by tactical
  duplication: compiled obligations, the generated coordinator prompt, workspace guidance, witness
  fields, the harness skill, cockpit recipes, active ledger prose, and exact-wording tests each
  repeated a locally plausible process until the combined route rewarded packet compliance and
  terminalized first divergence. Immediate recovery compressed those active layers around the
  outcome, known evidence, real constraints, useful tools, and the hard binding/receipt/
  contradiction/credit/cleanup/terminal-state kernel; it preserved prior evidence and left the
  coordinator and worker responsible for causal strategy. The repeatable correction is to protect
  machine-owned truth mechanically and test observable behavior, while treating diagnosis, repair,
  action order, instrumentation, and reruns as agent judgment. Installed policy/prompt tests now
  admit different strategies while still rejecting invalid durable transitions. Messy-playtest
  counterexamples prove that classified and permissive choices can continue without hiding their
  receipts, while false binding, contradiction, and evidence promotion remain rejected. The
  adaptive-autodrive proposal was not implemented: the current worker-owned cockpit route already
  permits source/tool repair and the proposal supplied no failure that survived the corrected
  instruction path.

* Owner-authorized [trigger]: Stop routing R-005 around every monster the player happens to notice.
  Reassess the active R-005 causal model before continuing: distinguish the player/observer's
  auto-walk and wait path from the bandits' production-owned path calculation, and inspect current
  monster positions only to determine which actor and route they actually influence. The current
  qualification and waypoint scenarios have clairvoyance/night vision but no `DEBUG_CLOAK` or
  `observer_safety_mode: invisible`; repeated coordinate avoidance is therefore measuring an
  uncloaked player's hostile-sighting interruptions and may not test the intended bandit ecology.
  Preserve authentic findings, but retire or rewrite route-planner work that does not contribute to
  the intended natural bandit outcome.

  Add explicit, receipt-bearing danger-handling modes for both bounded wait and auto-walk in the
  cockpit/TUI. Preserve clear choices equivalent to: stop on every interruption; handle only
  classified non-dangerous interruptions; or ignore danger and interruption popups while waiting or
  auto-walking. The third choice is an ordinary optional per-operation LLM-facing control, not an
  approval gate: expose it, execute the agent's choice, and receipt what was skipped. Do not require
  cloak proof, scenario permission, a safety classification, or supervisor approval. A concise
  tooltip may say that cloaking is often useful with this mode. Reuse the existing `raw_wait`,
  guarded `keep_watch`, guarded/raw movement, native interruption handling, run binding, and receipts
  rather than building competing wait or travel engines. Trust the agent to select the appropriate
  mode from the current situation. Keep the default cautious, but do not make the permissive option
  fail closed merely because the harness would have chosen differently. Make all three choices
  obvious and independently available for waiting and auto-walking.

  Calibrate safety to playtesting rather than real-world harm. A disposable fictional character
  being spotted, bitten, injured, or killed is an observable game result, not an external safety
  incident. Do not teach workers or encode tests that stop merely because a generic `danger` or
  `damage` flag became true. The LLM may continue, ignore, retreat, heal, zap, restart, or inspect as
  appropriate to the playtest. Stop or invalidate only when the event contradicts the selected
  test's outcome, evidence boundary, identity/binding, or the agent's chosen mode—not because the
  harness applies blanket caution to fictional stakes.

  Allow narrowly scoped creature zapping when it is the simplest diagnostic or setup intervention.
  Bind it to an exact creature identity and position, record before/after state and cause, preserve
  cleanup, and grant zero natural-ecology, route, combat, lifecycle, qualification, or certification
  credit to the intervention. Use reassessed monster positions and zero-credit zap controls to learn
  whether bandit path selection changes, whether only the player's observer UI was interrupted, or
  whether the creature genuinely blocks production bandit behavior. A final natural claim must
  still come from an independent run without ecology-changing intervention; ignoring observer/player
  interruption UI may remain valid only when it does not alter bandit or monster state or decisions.

  Rewrite the DFS, active ledger, charters/scenarios, harness guidance, TUI contracts, and behavioral
  tests together around that corrected distinction. Prefer deletion of coordinate-churn machinery
  over another exception layer. Behavioral tests should show that each LLM-selected wait and
  auto-walk mode does what its label says, the default remains cautious, skipped interruptions are
  visible in receipts, a zero-credit zap reveals whose path changed, and a separate intervention-free
  natural run that alone can receive R-005 credit. Consume this trigger only when the current R-005
  trajectory no longer treats incidental player sightings as unexplained bandit-route blockers.

  Disposition — resolved by exclusive review `b95c62281c33`. The earliest preventable cause was an
  ownership error in the native-travel boundary: every event after a player-visible hostile boundary
  was rejected as route contamination, while the active R-005 scenario disabled continuation and the
  ledger responded by accumulating alternate coordinates. Those receipts honestly prove uncloaked
  player auto-walk interruptions, not production bandit path causality, so all completed runs were
  preserved as zero-credit evidence and the coordinate planner was retired from the active route.
  Immediate recovery added explicit cautious, classified-non-dangerous, and permissive modes to wait
  and movement contracts, kept the cautious default, and made handled prompts, fictional-danger
  decisions, and native hostile boundaries durable in results. Classified movement can consume only
  a bound safe recovery; permissive travel can answer only the exact native auto-move cancellation
  and records `N`. An R-021 exact-identity creature zap remains optional, receipted, and zero-credit.
  The repeatable method is now a causal comparison of observer control flow with production ecology
  state, followed by a separate intervention-free natural run as the sole qualification route. The
  counterexample uses one trace containing active, hostile-boundary, progress, and completion events:
  default handling rejects post-boundary travel, while explicit permissive handling reaches green
  and retains the handled boundary; inverse cockpit tests show stop mode takes no recovery action,
  classified mode handles a safe prompt, and permissive mode continues visible fictional danger.

## Earlier consumed suggestions

* Owner-authorized [defer]: Put cheap source/executable readiness before costly playtest authority
  or repair runs, without turning build order into a rigid ritual. In `R-009-closure-014`, the
  Terra/high worker correctly received “revalidate the current binding” and the macOS-first route,
  but `.agents/skills/caol-harness/SKILL.md` routes query contradictions directly into
  `registry-repair-bootstrap`, while `.de67/test-and-task-guidelines.md` says only that binding drift
  invalidates a receipt. The worker therefore consumed a repair run, diagnosed and repaired the
  unconditional-Escape startup defect, and only at the later launch boundary learned that executable
  source `7f740f8fc7` did not match repository source `adb27ff46d`. Evidence anchors are worker
  `01a04e24-1693-7b63-93f2-e7f0daf74ff0`, task `R-009-closure-014`, its final finding, and
  `build_logs/r009_closure014/`.

  Review the worker brief, harness skill, registry query/repair-bootstrap output, and owning tests as
  one information path. Make the earliest cheap preflight expose whether the selected executable is
  current enough for the intended run. When product-binary staleness would invalidate the next
  gameplay or harness conclusion, build or select a source-matching executable before consuming
  single-use authority or launching a repair/bootstrap run. Preserve agent discretion to use a stale
  executable for a deliberately isolated diagnosis whose conclusion does not depend on current
  product code; label that evidence provisional and still revalidate on a current build before
  claiming the playtest outcome. Prefer one actionable status/next action over duplicated prose or a
  new approval gate. Add a counterexample that starts with a stale executable and proves the route
  requests the build before launch, plus a control showing that an explicitly isolated harness-only
  diagnosis is not unnecessarily blocked.

  Disposition: consumed by exclusive incident review `c94a5e1b3eb0`. Immediate recovery preserves
  the unconditional-Escape repair, its focused tests, accepted cleanup, revised gap 6, and zero wait
  or feature credit; the stale binary now receives `build_required` before repair authority. The
  incident came from an information-path defect, not worker capability: query-time binding paired
  current source with an arbitrary executable, the harness skill routed the contradiction directly
  to repair bootstrap, and the authoritative compiled-head comparison appeared only after startup.
  The repeatable correction reads the existing executable `--version` surface before query-bound
  repair, reports one actionable source/executable status, and refuses repair-token issuance until
  product-binary inputs match. Explicit harness-only diagnosis remains available with a provisional
  ceiling and required current-build revalidation. The original `7f740f8fc7` versus `adb27ff46d`
  counterexample names changed `src/bandit_live_world.h` and `src/game.h` and requests a build before
  launch; the isolated control remains unblocked without gaining playtest credit. Sixty focused
  readiness, registry, charter, and capture-owner tests passed.

* Owner-authorized [defer]: Give mutation review an honest `no change justified` outcome and apply
  the alignment manifest to the active de67 3 instruction system. A scheduled, random, universal,
  reviewer-authored, or general guideline review currently pressures the reviewer to edit something
  merely because a review was opened. That manufactures method churn and lets tests accumulate as
  constitutional rules. The reviewer must be able to inspect the active route, conclude that the
  present method is already aligned or that the proposed change lacks evidence, record a concise
  no-change disposition, resolve the review normally, and return control without altering DFS,
  ledger, policy, guidance, scripts, or tests.

  Preserve the authority distinction. `No change justified` is valid for automatic reviews and
  lesser-authority reviewer proposals. It is not permission to silently ignore an explicit
  owner-authorized outcome. For an owner-authorized suggestion, either achieve the requested
  outcome or preserve the entry with the exact conflict, impossibility, missing authority, material
  risk, or unresolved uncertainty. A no-op review may still complete the exclusive reviewer
  lifecycle and hand control to the required fresh coordinator; it must not invent a code diff as
  proof of diligence.

  Use the optional alignment-audit method as a review lens, not production prompt cargo or a fourth
  phase. Inspect only active de67 3 and repository instruction routes, the machine output presented
  to agents, and tests enforcing those routes. Classify active authority, useful reference, and
  discoverable legacy residue. Trace every suspected problem through its exact runtime branch before
  changing anything. The earlier claim that `adaptive_semantic_autodrive` stole control from the
  worker is a mandatory rejected counterexample: tracing showed it drives declared semantic-window
  setup, while `cockpit_live_session` blocks in `serve_cockpit_live` and hands actions to the LLM.
  Do not revive or encode that false finding.

  Recheck the alignment findings that did survive the evidence pass and make only changes whose
  deletion would leave the live workflow materially misaligned:

  Evidence receipts below are investigation anchors, not automatic verdicts. Resolve paths against
  the installed de67 3 skill and this CAOL workspace, recheck current symbols when line numbers
  drift, and trace callers before editing:

  - Corrective-recovery mismatch: installed
    `de-67-3/scripts/coordinator_supervisor.py:804-833` describes later corrective opportunities,
    while `:1451-1461` immediately returns failure for an unchanged fingerprint. The active tests
    `de-67-3/tests/test_coordinator_supervisor.py:755-803`
    (`test_unchanged_success_with_executable_work_is_not_resumed` and
    `test_persistent_crash_without_durable_progress_is_not_retried`) encode the immediate-stop path.
  - Text-shaped progress and routing facts: installed
    `de-67-3/scripts/coordinator_supervisor.py:372-425` hashes raw DFS, ledger, mutation-ledger, and
    compiled-policy bytes into `supervision_fingerprint`; installed
    `de-67-3/scripts/policy_kernel.py:719-809` derives work from any ledger heading, ordinary prose
    phrases, and any DFS red glyph. These are the anchors for quoted-history, whitespace,
    waiting-section, and clerical-edit counterexamples.
  - Hardcoded worker strategy: installed `de-67-3/scripts/policy_kernel.py:299-379` emits one exact
    `gpt-5.6-terra`/`medium` spawn example although
    `de-67-3/scripts/coordinator_supervisor.py:845-857` delegates Luna/Terra and effort choice to the
    coordinator. `de-67-3/tests/test_policy_kernel.py:737-810` freezes the exact spawn shape and
    asserts Terra at line 778.
  - Test-as-policy candidates: installed `de-67-3/tests/test_policy_kernel.py:140-146` asserts that
    compiled policy is smaller than Markdown; `:869-879` preserves historical-baseline behavior;
    `de-67-3/tests/test_policy_compare.py:31-44` carries the legacy divergence list; and
    `de-67-3/tests/test_coordinator_supervisor.py:1057-1097`, `1119-1213` assert substantial prompt
    content. Determine which assertions protect observable behavior and which merely fossilize one
    implementation.
  - False service health: installed `de-67-3/scripts/supervisor_service.py:247-255` asks tmux for
    pane fields while targeting the session. The observed live response was
    `pid= created= windows=` even though direct process inspection showed the supervisor and Codex
    child alive. `de-67-3/tests/test_supervisor_service.py:323-370` accepts any output containing
    `pid=` at line 359 rather than requiring numeric/process truth.
  - False cockpit TUI health: workspace `tools/openclaw_harness/cockpit_tui.py:265-295` defaults
    missing status to `active` and a clear error; `:316-325`, `349-353`, and `369-384` discard or
    fail to surface unsuccessful `run.status` responses after refresh, dispatch, and finish.
    `tools/openclaw_harness/cockpit_tui_test.py:155-165` covers failed observation, but not a
    successful observation followed by failed status.
  - Rejected autodrive finding: workspace
    `tools/openclaw_harness/startup_harness.py:23311-23443` constructs and blocks in the
    worker-controlled `cockpit_live_session`, whereas `:24186-24227` applies semantic autodrive only
    inside `adaptive_semantic_window`; `tools/openclaw_harness/scenario_registry_cli.py:1955-1974`
    sets the flag for the canonical probe. Preserve this mode distinction.

  - Treat tests as executable explanations, not constitutional authority. Preserve mechanical
    invariants such as identity, ownership, leases, single-use receipts, immutable evidence,
    cleanup, process reaping, restart acknowledgement, truthful lifecycle, and fail-closed parsing.
    Rewrite, demote, or delete active tests that freeze exact prompt prose, one worker model or
    effort, one strategy or task order, proof-shaped ceremony, a historical baseline, obsolete
    compatibility, arbitrary projection caps, or token-size aesthetics without an authoritative
    contract. Test behavior and concrete false outcomes rather than wording.
  - Reconcile documented corrective recovery with runtime behavior and tests. A recovery contract
    that promises corrective opportunities must not immediately hard-fail the first unchanged
    frontier; conversely, any retry or fuse must come from existing authoritative policy and must
    stop repeated state rather than prescribe coordinator strategy.
  - Derive executable ledger work and durable progress from structured active state and meaningful
    transitions, not arbitrary headings, quoted history, stray red glyphs, whitespace, or raw
    Markdown byte hashes. Add negative controls for historical examples, waiting sections,
    clerical-only edits, and unchanged executable frontiers without turning the parser into another
    prose bureaucracy.
  - Make dashboard and TUI health truthful when process or status metadata is absent. The service
    currently permits empty tmux fields to satisfy a weak `pid=` assertion, and the cockpit TUI can
    replace a failed status read with an apparently active state. Require real numeric/process
    evidence or render state as unavailable/stale; keep durable coordinator, worker, and mutation
    roles distinct.
  - Let the coordinator choose among allowed worker profiles. Machine-check allowed model,
    ownership, and binding rather than hardcoding Terra/medium or one exact spawn sentence when
    coordinator judgment has selected another supported profile.

  Before promoting a candidate, run behavioral messy-state controls that admit multiple correct
  agent strategies and include a genuine no-change review. Report which suspected findings were
  rejected and why. The best valid outcome of this review may be a smaller change set than this
  list, including no method change for any item not supported by the active runtime path.

  Disposition: consumed by mutation review `b58bd8d728b7`. Immediate recovery made service and TUI
  unknowns unavailable instead of active, removed hardcoded worker strategy, and let no-change
  reviews close honestly. The repeatable correction now derives routing and progress from structured
  active state, preserves workspace-local policy at restart, and uses the existing three decision
  opportunities against semantic state. Behavioral counterexamples passed. The unavailable legacy
  comparison became optional, token-size and arbitrary projection-cap assertions were removed, and
  prompt tests now retain only operational invariants. The autodrive allegation was rejected after
  caller tracing confirmed that live cockpit play remains worker-owned.

* Owner-authorized [trigger]: Reorder the remaining CAOL proof trajectory around its actual
  dependency chain and restore every unfinished playtest claim to a visible lawful route. The
  current active ledger bundles R-009 macOS gameplay evidence with Linux/WSL and Windows compiler
  work, so the coordinator selected provisional cross-platform repairs while shared harness and
  gameplay work can still change the code they validate. Preserve the current in-flight compiler
  result as a checkpoint; do not kill it, discard it, or claim final platform proof from it.

  Structurally rewrite the DFS projection and active work ledger so the ordinary trajectory is:
  finish or repair the shared playtest harness and agent-facing playtest route; complete the
  remaining macOS and ecology/gameplay witnesses; settle the resulting gameplay and harness code;
  establish a reviewable source/binding freeze for those outcomes; then run final Linux/WSL and
  Windows compilation, preflight, and platform witnesses against that settled boundary. Split
  R-009's platform-independent/macOS playtest outcome from its final cross-platform validation so a
  compiler lane cannot outrank unfinished gameplay merely because both currently share one red
  ledger item. Preserve a narrow exception only when a platform compile is proved necessary to
  repair the shared harness or determine the gameplay implementation itself; state that causal
  dependency explicitly instead of treating general platform parity as parallel filler.

  Audit every red DFS claim against the ledger. Do not remove, accept, or hide an unfinished claim
  merely to simplify the projection. R-005, R-008, R-009, R-012, and R-023 must each retain a
  visible next action, accepted subproofs, and honest wake condition until their playtest outcome is
  settled. In particular, investigate whether R-008's manifest review and R-012's draft review are
  genuinely human or third-party authority. If they are repository-owned exclusive reviewer roles,
  route them to the correct agent-owned review mechanism rather than `Waiting on external
  authority`; if a real external owner exists, name that owner and the exact unavailable decision.
  Current evidence already shows R-008 is orphaned: `_exclusive_source_review_state` can require a
  decision and `record_source_bound_review_decision` can store one, but the latter is called only by
  a unit test, the public scenario-registry CLI exposes no source-review command, and the supervisor
  dispatches no source reviewer. Treat `external review` in that store function's docstring as a
  historical design claim, not proof that a real external owner or wake event exists. Trace R-012's
  draft-review path with the same caller-and-wake test. A waiting state with no named owner and no
  executable wake mechanism is a DFS/workflow gap and must be repaired or removed.
  Josef then explicitly authorized the current Codex session to perform both reviews. R-008 exact
  source revision 3 was accepted after its focused source/review and registry/observation controls
  passed; the registry now reports it executable and token-eligible. R-012's draft was reviewed as
  already pointing to an existing valid, no-review-required, zero-credit bootstrap scenario with no
  missing requirements, so its fictional review blocker was removed and its bootstrap plus later
  live validation were restored as ordinary active work. Preserve these decisions while removing
  the orphaned review mechanism from the future workflow.
  Preserve R-006 as the later certification-plus-Josef Windows-feel gate without letting it pull
  final platform validation ahead of the playtest freeze.

  Keep the coordinator free to change tactics inside this dependency order. Encode causal
  prerequisites and visible outcomes, not a brittle task script or exact claim-selection sequence.
  Preserve all authentic runs, findings, accepted evidence, dirty-worktree ownership, technical
  authority boundaries, and current source changes. Prove the rewritten projection with messy
  cases: an unfinished harness repair keeps final platform validation waiting; a completed
  playtest that changes shared code invalidates earlier provisional compiler proof; an in-flight
  platform build returns without being discarded; an agent-owned exclusive scenario review does
  not become a human blocker; a genuinely human Windows-feel judgment remains external; and every
  red DFS claim is either executable or has a truthful named wake condition. Promote the DFS,
  ledger, policy, and tests together only after the guarded route selects playtest/harness work
  before final cross-platform certification under the current frontier.

  Disposition: consumed by mutation review `b58bd8d728b7`. R-009 now has an active upstream macOS and
  shared-behavior route plus a separately dependency-gated final Linux/WSL and Windows route.
  Provisional compiler evidence is preserved without final credit and must be revalidated after the
  reviewable input freeze. R-005, R-008, R-009, R-012, and R-023 remain visible; R-008 revision 3 was
  durably accepted for execution, R-012's no-review bootstrap is executable, and append-only gap
  revisions remove both orphaned reviewer dependencies. R-006 remains the named external Josef gate.

### Earlier consumed suggestions

  - Owner-authorized [trigger]: Eradicate the false human-permission gate for ordinary agent-owned
  playtests and align the DFS, active work ledger, CAOL harness guidance, charters, tests, and runtime
  authority language with the trust-the-agent model. This is a structural authority-model repair,
  not permission for a search-and-replace or another sentence layered over contradictory rules.
  Research the complete live route before editing: coordinator outcome selection and witness-charter
  authorship; typed scenario query and selection; selection token, descriptor, cockpit/run receipt,
  canonical launch and cleanup; worker observation/action/repair/rerun; immutable journal and cited
  witness; evidence ceiling and registry ingestion; coordinator causal judgment; certification; and
  the separately owner-reserved Windows-feel judgment. Identify which component owns each decision
  and distinguish technical single-use run authority, evidence/proof authority, and genuinely human-
  reserved authority.

  Use the observed failure chain as a mandatory counterexample. The frozen DFS section
  `Competing systems and override direction`, row `Scenario selection and run opening`, says the
  worker owns fit judgment and may open one tracked run from a valid selection. The live-playtest
  section says the coordinator supplies a charter and the worker controls native play and acts as
  witness. In `.agents/skills/caol-harness/SKILL.md`, `Explicit selected launch and report` says the
  descriptor supplies authority, but the older sentence `Only after an explicit request to run the
  selected scenario` was read as a fresh human-permission requirement. R-009 workers
  `R-009-exploration-004` and `R-009-exploration-005` received no runtime permission rejection and
  never attempted a launch; the first inferred external authority from that ambiguous guidance, the
  coordinator projected the inference into `.de67/work-ledger.md`, and the second merely repeated
  the ledger. Commits `16bfadc64f` and `7f740f8fc7` then embedded `explicit selected-run authority`
  into R-009/R-005 charters and tests, converting the mistaken inference into repository law. Treat
  those references as evidence to investigate, not as an exhaustive edit list.

  Restore one coherent rule: when the coordinator selects an executable playtest outcome and
  supplies or authorizes the matching validated witness charter, that brief is the explicit request.
  The trusted worker may select the fitting scenario, claim and consume the mechanically issued
  single-use binding-specific run authority, launch, observe, act, repair, rerun when the binding and
  outcome remain valid, finish, and submit an objective evidence-cited witness without asking Josef
  again. The coordinator may create or repair the charter and judge the witness. Missing charter or
  invalid selection is repository/coordinator work or an honest technical finding, not automatically
  an external-owner blocker. Reserve human approval only where the DFS expressly assigns the outcome
  to a human, such as Josef's distinct Windows-feel judgment, or where system/platform safety truly
  requires it.

  Rewrite the active work ledger to remove the false R-005/R-009 external-authority blockers and
  remove the temporary standing-permission workaround once the native model makes it unnecessary.
  Reconcile and refreeze the DFS so its authority tables, playtest/witness contract, red claims, and
  executable routes agree. Rewrite the CAOL harness skill and any coordinator/worker routed guidance
  so query-only inspection cannot accidentally launch, while an explicit coordinator playtest brief
  plus validated charter is sufficient. Audit all current charters, honest-stop conditions, tests,
  token/descriptor names, CLI messages, dashboard text, and ledger projections for the same
  conflation. Delete, rewrite, or demote tests that assert human permission; do not preserve a faulty
  workflow merely because a regression test currently encodes it. Promote further research and
  context alignment wherever the surrounding mechanism reveals a more general mismatch.

  Preserve the useful hard boundaries: single-use source/executable/scenario/world/owner binding;
  no launch from an inert query or draft; exclusive process/world ownership; immutable native
  observations, actions, receipts, cleanup, citations, and contradictions; honest stop on binding
  drift or unsafe/unknown state; evidence ceilings; no setup, diagnostic, focused, or witness prose
  self-promotion; and separate coordinator causal judgment. Trust the agent's actions because the
  instructions and information are correct; mechanically police only machine truth and the few
  boundaries whose failure could fabricate evidence, corrupt shared state, or consume the wrong run.

  Prove the repaired model with behavioral scenarios, not exact prose matching: query without a
  playtest brief stays non-launching; coordinator brief plus validated charter lets a worker obtain
  and consume one bound run receipt without human intervention; a missing charter routes to charter
  creation/repair rather than Josef; a changed binding invalidates the receipt and permits a fresh
  agent-selected retry after revalidation; the worker can honestly conclude proved, contradicted, or
  inconclusive from cited evidence; supplied contradictions and cleanup failures remain visible;
  clerical defects are repaired without replay; certification and Windows-feel ownership cannot be
  stolen; and the historical R-009 chain cannot recreate an external-authority ledger blocker when
  no harness rejection occurred. Guard and promote the aligned DFS/ledger/guidance/harness candidate
  together, consume this trigger only after stale authority language and tests are removed, and
  request one fresh coordinator at the safe mutation boundary.

  - Disposition: consumed by exclusive owner-suggestion review `9f2e30b20829`. The incident began
    when ambiguous harness guidance split an already selected coordinator outcome from its run
    request; two workers made no launch attempt and received no runtime rejection, but the inferred
    human gate was projected into the ledger and then frozen into charters and exact-prose tests.
    Immediate recovery removes those false external blockers from R-005, R-009, and R-023. The
    repeatable correction gives the coordinator brief plus matching validated charter ownership of
    the execution request; keeps query inspection inert; gives binding-specific, single-use
    technical run authority to the registry; and reserves human authority for outcomes the DFS
    expressly assigns to a human. The aligned DFS, ledger, routed guidance, harness skill, charters,
    runtime language, tests, and compiled policy were compressed around that ownership split.
    Counterexamples prove that query selection alone does not launch, a brief-requested charter can
    consume a bound token without a human-permission parameter, binding drift still fails closed,
    and contradictions, evidence ceilings, immutable witness history, and cleanup remain enforced.
    The guarded 15-rule policy compiled reproducibly with guard digest
    `76f338c9b05ba3595d81c3a66e842b5eeac7590395f84f99fccd28efa4b1f68c`; 79 focused authority,
    charter, witness, ingestion, and cockpit tests passed.

  - [defer] Reviewer-authored proposal: Define the authority model for R-006 Windows feel evidence.
  The current route stores an immutable pass or fail and binds it to the certified executable, but
  any local caller can submit the literal label `Josef`. This matters because a forged row can make
  the combined acceptance green. The DFS must choose one of two coherent contracts. It can name a
  trusted identity authority and verifier, including enrollment, revocation, replay resistance, and
  handoff and build binding. Or it can declare Windows feel an external non-machine-verifiable
  judgment and prohibit that row from mechanically making `overall_acceptance` green. Preserve the
  ordinary pending handoff, executable binding repair `483c772211`, immutable storage, and separate
  automated gate. Do not treat an author string, mutable Git identity, or unavailable local signing
  configuration as authentication.

  - Disposition: consumed by exclusive owner-suggestion review `9f2e30b20829`. Repository search
    found no authority capable of authenticating Josef, so the smaller supported contract is the
    external, non-machine-verifiable judgment model. The registry still binds and immutably records
    an external pass or fail against the certified Windows handoff, while machine eligibility never
    turns `windows_feel` or `overall_acceptance` green from that row. The counterexample supplies a
    registry-shaped fact carrying both `authority=windows-josef` and `owner=Josef`; eligibility
    remains false with reason `external_windows_feel_not_machine_verifiable`. A separate ingestion
    test proves a labelled pass remains visible as an external attestation without satisfying the
    automated gate. R-006 remains honestly red until the real external product judgment occurs.

  - Owner-authorized [defer]: At the next regular mutation review, inspect and reconcile the three
    cautions from the latest live trajectory audit, then make the smallest evidence-backed changes
    needed to both the frozen DFS and active work ledger. First, examine R-005's seven consecutive
    findings without closure and distinguish cumulative discovery of real route/ecology constraints
    from repeated strategy or administrative churn. Preserve useful evidence, but rewrite or
    consolidate its DFS gap and ledger spokes if their current shape encourages replay instead of a
    materially different next move. Second, reconcile R-022's accepted ledger state with the DFS
    projection that still presents it as red. Preserve its zero-credit ceiling and do not broaden
    its accepted item-spawn result into gameplay or economy proof. Third, inspect the very large
    dirty worktree and accumulated run artifacts as an auditability and trajectory-management
    problem: reflect any genuinely unfinished integration, cleanup, or release-boundary work in
    outcome-sized DFS/ledger entries, while removing stale, duplicate, superseded, or purely
    clerical projections that no longer represent required work. Do not turn repository tidiness
    into a new proof bureaucracy, discard authentic evidence, replay settled gameplay, or expand
    scope merely because files are dirty. The review is complete only when DFS and ledger agree on
    what is accepted, what remains red, why it remains red, and the next materially useful route.
  - Disposition: consumed by exclusive review `063affb459d9`. R-005's seven latest findings were
    cumulative causal discovery, not seven unchanged retries, but each unsafe run stop was
    terminalized and the latest counterexample left SQLite revision 28 asserting a final-handoff
    defect that it never reached. Immediate recovery appended revision 29, rewrote the DFS and
    canonical ledger around zero-credit qualification of a materially different natural route, and
    forbids repairing the handoff until that boundary is reproduced. The selected task guideline now
    distinguishes ending unsafe gameplay input from terminalizing an authorized diagnosis/repair
    task. R-022 now reflects durable acceptance `R-022-closure-004` in the DFS while preserving its
    setup-only ceiling. The trajectory audit found a real checkpoint boundary—56 tracked status
    entries, 906 untracked entries, 1,684 build-log files, and 688 runtime run directories—so R-025
    owns reviewable checkpointing and evidence retention without requiring a clean tree, artifact
    deletion, or settled-gameplay replay. Duplicate R-019 projection prose was consolidated, and the
    previously noncanonical active ledger changed from zero machine-visible items to five guarded
    slice briefs. Counterexamples prove revision 29 preserves the closure-029 interruption and
    rejects replay repair; the R-022 completion guard, cycle-15 lane guard, live ledger validator,
    five slice extractions, and `git diff --check` pass. No authentic evidence or product code was
    removed or changed by the review.

  - Reviewer-authored proposal [defer]: Add a named machine slice for R-015 to the frozen DFS. The
  compiled policy rejected `R-015-exploration-001` because R-015 has no `DFS-SLICE` binding. This
  matters because a deadline task can be created but cannot be bound to an ordinary worker, so the
  lifecycle-boundary ordering defect cannot be investigated or repaired. Preserve the existing R-015
  outcome and proof text. Add the smallest stable slice identifier and claim binding that let the
  policy pass only the R-015 contract to an exploration or closure worker. Prove the kernel emits a
  valid deterministic spawn brief for R-015 before marking this suggestion resolved.
  - Disposition: consumed by exclusive review `0f4add0c7800`. Immediate recovery added marker-only
    slice `R-015-S001`, reprojected R-015 from mutation wait to executable exploration, and preserved
    its outcome, proof, and all prior evidence. The task failed because R-015 was refrozen while not
    active, but mutation closeout required slice proof only for active claims. The repeatable
    correction requires every added or changed red claim—projected, waiting, or unselected—to yield
    a lawful slice brief before restart. The reviewer entry also used a human-readable defer label
    outside the parser grammar and was treated as a legacy trigger; the compressed ledger protocol
    now places `[defer]` first for lesser-authority proposals. Copied-state counterexamples prove the
    original R-015 task fails before the marker, the corrected state emits an isolated deterministic
    R-015 spawn brief, a correctly encoded reviewer defer does not retire the coordinator, and an
    owner trigger still does.

  - Owner-authorized [trigger]: Repair the R-018 closure contract after the witness replacement.
  The current gap still requires a green R-018 matrix, but the accepted replacement deliberately
  removed that matrix and forbids keeping both proof systems. No R-018 charter now defines which
  preserved raw, primitive, master-off, and gadget-off facts form a mechanically valid immutable
  witness. This matters because authentic native receipts cannot become admissible proof, and new
  gameplay runs would reproduce the same contradiction. Rewrite the R-018 gap and its executable
  decomposition around one coordinator-authored charter, cited immutable journals, contradiction
  disclosure, evidence ceilings, and coordinator judgment. Preserve the existing raw and primitive
  receipts. Do not restore the retired matrix. Require the repaired route to state exactly which
  off-role facts remain necessary before another ordinary worker is dispatched.
  - Disposition: consumed by exclusive review `c6eefc49a809`. Immediate recovery rebased open
    gap `R-018/G-001` from the retired matrix to one coordinator-chartered composite witness and
    projected the executable subtasks without replaying accepted raw/primitive reports. The
    remaining off controls are exactly master-off and raw-wait-gadget-off; each must prove no gadget
    native dispatch, preserved primitive acceptance and target progress, no interpreted
    interruption, successful cleanup, and a focused ceiling. The incident occurred because the
    witness replacement changed DFS, harness, and ledger method owners but left SQLite gap revision
    9 authoritative. The repeatable correction makes proof-owner replacement set a conditional
    projection-rebase obligation that blocks mutation resolution until affected durable gaps and
    active briefs no longer name the retired owner. Policy contracts accept the rebased trace and
    reject the original suggestion-disposition-plus-stale-gap transition.

  - Owner-authorized [trigger]: Replace playtest proof bureaucracy with an LLM witness channel and
  route its instructions to the agents that own the judgment. This is a structural reduction, not
  another report layered over existing matrices. Research the complete live path first: coordinator
  playtest recognition and dispatch brief, worker skill routing, cockpit/TUI descriptor and event
  journal, immutable run artifacts, registry ingestion and evidence ceilings, coordinator result
  judgment, DFS acceptance, and every scenario-specific proof matrix or step-ledger owner that the
  replacement can delete.

  This mutation explicitly authorizes and requires a **DFS rewrite and active work-ledger rewrite**,
  not merely a policy, prompt, harness, or schema patch. Recast playtest work as outcome-sized DFS
  tasks with visible internal spokes. When an outcome genuinely decomposes, one task should contain
  **4-7 coherent, independently visible subtasks** covering the useful arc (for example preparation,
  authentic execution, observation, causal judgment, repair when needed, and acceptance), rather
  than opening a succession of single-subtask clerical tasks. Preserve fewer subtasks for work that
  is honestly smaller; never invent filler to reach the range, merge unrelated outcomes, or turn a
  task into an unbounded bucket. A one-subtask task is appropriate only when the outcome is genuinely
  atomic. Rewrite the current ledger into this shape so the sidecar plot visibly renders meaningful
  spokes and the coordinator can continue toward the outcome without detaching and reopening the
  task after every small step. Make task closure mean that the outcome is settled, while subtask
  state communicates real progress and remaining uncertainty.

  Put the coordinator-facing policy in the routed coordinator playtest contract, not in a large
  supervisor prompt. When the selected ledger work requires a live playtest, the supervisor may
  inject only a compact deterministic pointer that tells the coordinator to use that contract and
  name the relevant harness skill; it must not prescribe the test trajectory or police the model.
  The coordinator drafts a short witness charter containing the claim in plain English, what would
  materially prove or contradict it, already-accepted evidence, the current uncertainty, forbidden
  shortcuts or credit, and honest stop conditions. It describes the proof question rather than one
  exact interaction transcript. The coordinator gives that charter to the worker and later judges
  the returned witness against authentic evidence.

  Put worker-facing operating and witness guidance in the CAOL harness skill and expose the same
  compact contract through the live cockpit/TUI only when a playtest is active. The worker owns
  observe, choose, act, repair, rerun when necessary, finish, and witness. It receives a compact
  immutable evidence journal with scenario/source/executable/run identity, native observations,
  dispatched actions and receipts, state deltas, interruptions, cleanup, evidence ceiling, and
  visible contradictions. It writes one evidence-cited witness statement with: the smallest
  supported claim; verdict `proved`, `contradicted`, or `inconclusive`; causal account; cited
  observations and their meaning; material deviations; contradictions; remaining unknowns; and
  recommended accept, continue, repair, or change-strategy disposition. Instruct it explicitly:
  report the smallest conclusion supported; distinguish observation, inference, contradiction, and
  unknown; preserve inconvenient evidence; do not request more proof after the claim is honestly
  settled. The TUI may expose a generic `WITNESS / FINISH` action, but must not force a fixed prose
  template before ordinary gameplay actions can proceed.

  Retain only mechanical truth owners: source, executable, scenario, selection, run and ownership
  binding; immutable native event/action/cleanup receipts; requested-to-executed action correlation;
  append-only final artifacts; citation existence and value checks; contradiction visibility; and a
  ceiling preventing setup, intervention, diagnostic, or focused evidence from earning broader
  credit. The LLM interprets causal sufficiency, semantic equivalence across independent runs,
  materiality of deviations, whether another run is necessary, and what remains unproved. A witness
  cannot generate facts, cite absent receipts, conceal a supplied contradiction, or promote its own
  evidence class.

  Give the coordinator this governing review rule: **intellectual scrutiny, administrative
  leniency; challenge the conclusion, repair the paperwork.** Judge proof by causal evidence, not
  conformity to an expected packet. Before rejecting a witness conclusion, name the concrete false
  conclusion, unresolved causal ambiguity, unsafe action, identity failure, contradiction, or false
  promotion that the missing or malformed information could permit. If none exists, preserve the
  proved result and derive, normalize, annotate, or repair the clerical record without replaying
  gameplay. Never rerun gameplay merely to repair documentation when the immutable evidence still
  contains the required truth. Conversely, polished prose never overrides a contradictory receipt,
  missing authentic identity, or absent causal observation.

  Prove the replacement first through R-018 as the smallest live vertical slice: coordinator charter
  -> worker-controlled raw/primitive/off playtest -> immutable event journal -> witness statement ->
  citation/integrity validation -> coordinator acceptance. The witness may explain expected run-local
  identity differences and semantically equivalent advertised native actions without teaching a
  bespoke matrix every field in advance. Preserve any genuinely necessary inverse control and source
  binding. Once this route proves the same R-018 outcome, delete or demote to optional analytics the
  superseded R-018-specific matrix, duplicated red/yellow/green ledgers, exact run-local cross-run
  equality, forced menu sequence, report-only rerun triggers, and special ingestion policy. Do not
  retain both proof systems. Then adapt R-019 using separate precondition-compatible safe and unsafe
  starts; do not revive the contradictory timed-entry dog route.

  Behavioral tests must allow multiple correct action sequences and witness wording while proving:
  correct causal proof with an optional or derivable field missing is accepted and normalized; a
  semantically equivalent native sequence is accepted; a nonexistent or mismatched citation is
  rejected; an omitted supplied contradiction is rejected; a source/executable/run or ownership
  mismatch is rejected; missing clerical summaries are derived without replay; a genuinely absent
  causal fact remains inconclusive; setup/intervention evidence cannot promote itself; and the
  coordinator receives the routed charter/judgment guidance while non-playtest dispatches do not pay
  for or receive the full playtest manual. Guard and promote the complete same-outcome policy/DFS/
  guidance/harness candidate together, consume this trigger only after the replacement route and
  deletion are proved, request one fresh coordinator, and preserve current authentic run evidence.
  - Disposition: consumed by exclusive review `892e4aaae887`. Immediate recovery replaced the
    active R-018/R-019 matrix gate with chartered immutable witnesses while preserving prior reports.
    Repeatable correction added outcome-sized durable subtasks, nonterminal checkpoints, sidecar
    spokes, playtest-only coordinator routing, and worker/coordinator witness contracts. Focused
    counterexamples prove independent-run semantic comparison, optional-field normalization,
    citation/value and contradiction rejection, identity/digest and ceiling failure, missing-fact
    inconclusion, generic registry persistence, coordinator review separation, and absence of the
    playtest manual from unmarked dispatches. Policy guard digest:
    `b3f1d2e28fb4409173c5f24717f12ccdaa245e14eb8fb9addd50aba1753a6af5`.

* Owner-authorized [trigger]: Add automatic recoverability check-ins for the Phase-3 product
    workspace, using its configured source branch and single push target just as method work is
    durably committed and pushed to `de67-lab`. The C-AOL `dev` branch exists for active development,
    so a recovery checkpoint does not require owner approval or polished hand-written commit prose.
    It must remain explicitly distinct from DFS acceptance, tested integration, promotion, or
    release.

    Keep exactly two publication domains. C-AOL product code and its repository-owned playtest
    harness, witness contracts, fixtures, tests, and workspace-local DE67 delivery artifacts are one
    product domain and checkpoint together on C-AOL `dev`. Reusable DE67 method source, packaged
    assets, and method tests are a separate `de67-lab` domain and checkpoint only to that repository.
    Never mix either domain's files, commit, ref, receipt, or push target into the other.

    Check in only at a real quiescent lifecycle boundary after durable state shows no live ordinary
    worker or reviewer editing that repository. Include tracked changes and new non-ignored source,
    test, harness, policy, and documentation files. First make repository ignore rules accurately
    exclude generated builds, run logs, saves, coordinator state, screenshots, and other reproducible
    bulk so they cannot enter the commit merely because they are untracked. Derive a concise,
    deterministic recovery message from the DE67 lineage and terminal transition; model-authored
    wording may improve it but must not be required for safety.

    Perform the check-in as supervisor or harness infrastructure outside model context. Do not ask a
    coordinator, worker, or reviewer to stage files, write a report, choose a message, inspect the
    diff for checkpointing, or spend a turn supervising routine Git durability. Agents continue to
    own implementation and proof judgment; the automatic check-in merely preserves their files.

    Make the Git and SQLite histories mutually discoverable without committing the mutable database.
    Allocate one stable checkpoint id in SQLite before staging, and put that id plus the lineage and
    relevant durable state revision in structured commit-message trailers. After Git creates the
    commit, finalize the same SQLite row with the commit SHA, source and target refs, remote, and push
    verification. A crash between those steps must be idempotently recoverable by reading the
    checkpoint trailer from HEAD and completing the existing row rather than creating another commit.
    Either side must retain enough identity to find and verify the other; do not depend on a mutable
    database pathname or paste verbose ledger/report text into the commit message.

    Push the resulting commit through the workspace's existing single-target configuration, verify
    that the remote `dev` ref equals the new local HEAD, and record a compact receipt. A failed add,
    commit, hook, push, or verification must preserve the dirty tree, surface a recoverability
    incident, and prevent silently accumulating another work interval without protection. Never
    reset, discard, amend, force-push, sweep another worktree, or treat a checkpoint as evidence that
    the changed behavior passed. Prove with temporary repositories that a quiescent C-AOL-style
    workspace commits and pushes modified and newly added product files, excludes generated bulk,
    makes no empty commit, refuses the wrong branch or multiple targets, detects a remote mismatch,
    reconciles a crash after Git commit but before SQLite finalization without duplicating the commit,
    resolves Git-to-SQLite and SQLite-to-Git identity, and does not check in while any worker or
    reviewer is live. Preserve the separate `de67-lab` publication route and install/hash-check the
    validated method change before resolution.

    Disposition (gate `72b2d68a242d`, lineage `semantic-surface-cockpit`, 2026-09-04): consumed.
    Immediate recovery preserves interrupted R-030 attempt receipt `a6286419…b61163`, its four
    untested harness edits, and its exact no-credit ceiling; the already abandoned attempt and
    released worker claim were not terminalized again. The active ledger now tells the successor to
    validate and complete those edits instead of discarding them or treating them as accepted. The
    product ignore boundary excludes `.de67/state`, the no-go archive, build logs, mutable registry
    storage, generated builds/saves, and captured screenshots while leaving new repository-owned
    source, tests, fixtures, scenarios, charters, and guidance visible to checkpointing.

    Incident reconstruction: workers and reviewers correctly left a large, valuable dirty frontier
    because Git durability was model-visible manual work and the supervisor's terminal journal had no
    repository transition. Generated bulk was also still eligible for a blanket `git add`, while Git
    and SQLite had no shared recovery identity. The earliest preventable cause was the missing
    supervisor-owned transaction after a quiescent journal close, not agent caution or commit-message
    quality. A later blanket reminder to commit would have repeated the same dependency on model
    attention and could not recover a crash between Git and SQLite.

    Repeatable correction: the external supervisor now invokes one standalone checkpoint transaction
    only after it closes the active coordinator or mutation-reviewer journal row and before another
    model interval. The transaction validates the configured local branch, exact single push target,
    and remote URL; scopes liveness to the current supervisor owner so dead historical journal rows do
    not create a false gate; allocates a stable SQLite checkpoint ID and state revision before staging;
    stages tracked changes and non-ignored new files; creates no empty commit; writes the identity,
    lineage, state revision, source ref, and target ref as structured Git trailers; pushes without
    amend, reset, or force; and verifies the remote ref. Commit, hook, push, or verification failure is
    durable and stops the next interval while preserving the tree. A pending allocation whose trailer
    is already on `HEAD` resumes the same row and commit rather than duplicating either.

    Reproduction and counterexamples: seven temporary-repository tests prove modified and new product
    files reach the configured bare remote while ignored bulk does not; an unchanged tree creates no
    commit; wrong branch and multiple targets fail before staging; a forced crash after Git commit but
    before SQLite finalization resumes the same checkpoint and commit; a forced post-push remote-ref
    race records failure rather than green; current worker or reviewer ownership refuses the
    checkpoint; and an unfinished row from a prior supervisor owner does not block the current
    quiescent boundary. A supervisor integration test observes zero unfinished current journal rows at
    checkpoint invocation. On this live state, the installed helper rejects the still-running
    `mutation-610cd…` reviewer itself, proving that installation did not smuggle the current model into
    product staging. The installed focused checkpoint/supervisor suite passes 77 tests.

    Method publication remains a separate domain: lab main was committed and push-verified at
    `13d72ab6…45fc0`; the installed release worktree, including the preceding compact-receipt method,
    was committed and push-verified to the `de67-lab` remote at `4849ed13…ab0b6`. The checkpoint
    implementation, tests, and supervisor reference hash-match between lab main and the installed
    skill. No product checkpoint was claimed during this live reviewer; checkpoint commits remain
    explicitly non-evidence and begin only at a supervisor-observed quiescent boundary.
