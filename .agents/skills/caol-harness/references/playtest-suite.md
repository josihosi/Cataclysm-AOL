# C-AOL product playtest suite

This is the recipe library and historical family index. The [work ledger](../../../../.de67/work-ledger.md)
owns assignments; the registry owns compatible scenario/build selection; run witnesses own proof.
A working recipe is reusable even when a game bug prevents its intended outcome.


## Current release selection

Use the [compact release catalogue](release-playtest-catalogue.md) for the eight maintained
recipes, original52-case reuse, movement refinements, expansion coverage and execution selection.
The categories and individual arms below describe reusable mechanisms, not another queued suite
or a requirement to run every historical variant. Keep stable IDs and historical verdicts;
current assignments/results belong in the ledger and task context. The original collection had
51 scoped passes and one E4B observed failure, not52 current-build successes.

## Performance and routing comparisons

Follow [performance responsibility](witness-and-finish.md#performance-findings-in-ordinary-playtests)
when current evidence contradicts maintained performance. This library does not dispatch historical
pending runs, preserve old game processes or select a routing redesign; current work belongs to the
existing assignment and task context.

Choose comparisons from the demonstrated cost: distinguish discovery, actor/light preparation,
site bootstrap, packet copying, diagnostic output and travel bookkeeping rather than naming their
aggregate as map scanning. Match workload, build, active populations and actual AI behavior. When
output is implicated, distinguish logical bytes from physical disk writes and preserve required error
evidence. When route reuse is implicated, select controls that expose invalid reuse or changed
geometry, bodies, occupancy and hazards. Useful comparisons must preserve actual journeys and
loading behavior; a cheap but behaviorally empty run proves no improvement. Do not turn these
examples into a required matrix, prescribe an architecture before diagnosis or replay old suites.

## Final long-running soak batch

After the planned mechanism/regression playtests, run the owner-selected
[five-seed natural-play soak](long-running-soak.md): at least seven actual game days and
four full save/exit/reload cycles per seed, varied travel and revisits, primarily checking
crashes, stuck dispatches and performance spikes/stalls. Use the soak protocol’s compact
timing references and targeted local-density/overmap-cost comparisons. Existing accepted coverage remains intact. Parallelism follows
measured capacity; the work ledger owns scheduling. Then select one useful world for the
[single-world month crash soak](long-running-soak.md#final-single-world-month-crash-soak),
continuing to30 actual game days total with minimal logging and brief failure-focused notes.

## Scenario intent and closed categories

Each scenario tests a gameplay question. Interpret its start/end conditions and recipe through
[the skill's test-design guidance](../SKILL.md#own-a-scenario-through-its-outcome), including when
an arm uses strict wording. Preserve conditions that make the question meaningful; adapt incidental
setup rather than turn examples into acceptance gates. A nearby camp can isolate signal reception
but cannot establish a believable long expedition; a routing test need not arrange visibility at an
arbitrary exact distance. Once the concrete scene is prepared, judge whether it actually exercises
the intended behavior and change the setup if it does not. Keep genuine failure and coverage limits
visible. The current assignment selects the relevant behavior and variants; these categories add no
runs and do not rewrite accepted evidence. There are six categories, with every family assigned once.
Cross-cutting loading, reload, platform, visibility and performance conditions remain variants.

### Signal, knowledge and response

**Families:** 1, 1v, 2, 3, 3c, 3r, 4r, 7, X3. **Question:** Does actual perception become grounded knowledge and a purposeful response?

**Finish:** Reach the selected observation, report/decision and encounter endpoint; ignition or dispatch alone is insufficient.

**Essential conditions:** Actual observer, channel, source age/identity, faction and receiver. Use [smoke and raid](smoke-raid-replay.md) and the individual arm below for concrete controls and variants.

### Encounters and threat response

**Families:** 3f, 5, 8, X2. **Question:** Do actors fight, flee, defend or exercise restraint for the danger they actually perceive?

**Finish:** Resolve the encounter and assigned aftermath; proximity or a menu alone is insufficient.

**Essential conditions:** Actual sight, capability, target, shots/damage/casualties and threat changes. Use [contact and withdrawal](rural-roof-replay.md#panic-and-withdrawal-regression) and the individual arm below for concrete controls and variants.

### Transactions and interrupted work

**Families:** 4, X1, X5. **Question:** Do real goods, payment and work survive interaction and interruption?

**Finish:** Finish settlement/return or resumed work, with the required persistence check and no loss or duplication.

**Essential conditions:** Actual payer, selected goods, destination loading, work activity and interruption. Use [payment branches](bandit-journeys.md) and the individual arm below for concrete controls and variants.

### Continuity across simulation boundaries

**Families:** 6a–6o (family 6), X4, X4b. **Question:** Do the same actors, knowledge, casualties and obligations survive absence, loading and reload?

**Finish:** Exercise the selected transition and downstream outcome; crossing a boundary alone is insufficient.

**Essential conditions:** Member identity, living/dead status, elapsed time, ownership, source provenance and physical custody. Use [6a–6o recipes](away-return-journeys.md) and the individual arm below for concrete controls and variants.

### Integrated survival

**Families:** X6. **Question:** Do ordinary actions compose into a coherent survival trip?

**Finish:** Finish leave/scavenge/trouble/return/rest/leave with consistent actors, items and orders. Judge needs only where selected.

**Essential conditions:** Native consequences and natural interactions; avatar walking itself is not an acceptance criterion. Use [X6 journey](#x6-longer-mixed-survival-trip) and the individual arm below for concrete controls and variants.

### Model-mediated companion behavior

**Families:** E4B. **Question:** Does the actual protected model understand and carry out supported commands in a continuous trip?

**Finish:** Judge command effects, revisions, danger and return/reload; distinguish model judgment from runner failure.

**Essential conditions:** Exact selected model/fixture, unchanged protected instructions, first attempts, request/NPC identity and native application. Use [protected companion protocol](llm-companion-journey.md) and the individual arm below for concrete controls and variants.
A category is covered only when its assigned constituent claims are answered, not when one
representative passes. A proved bug or harness blocker is a justified end to an attempt, with
concrete evidence and remaining coverage; it does not make an unfinished category pass. An event
that never occurred leaves that claim open. A legitimate refusal, defeat or retreat answers a
claim only when that is an allowed outcome of the mechanism actually exercised. Keep live counts
in existing results, not this taxonomy.

### What must stay real, and what can be setup

**Required behavior:** retain the selected scenario's causal chain and endpoint. **Essential
conditions:** preserve whichever variables distinguish that claim—actual sight, source height,
loaded owner, native elapsed time, item custody, danger or model input. **Flexible setup:** use
permitted avatar mutations, teleportation and prepared fixtures to remove unrelated effort.
Walking the avatar is not a default acceptance condition. A shortcut must not perform the tested
NPC action, fabricate knowledge, bypass the measured encounter or stand in for elapsed time.
The more specific restrictions in the owning protocol still apply, especially protected model
configuration and controlled casualty/fixture setup.

| Scenario / variant | Essential observation; latitude outside it |
| --- | --- |
| 1/1v, 3/3c/3r, 7 | Actual channel reception, watch/report and selected raid/contact. Preserve smoke-only, light-only, combined and ordinary/viewer distinctions. Avatar transit can be shortened; NPC ascent/contact and real exposure cannot be teleported into proof. |
| 2, 4r | Native admission, signal removal and subsequent routine observation/return. Empty observations can be correct; they do not prove populated-site or haul behavior. |
| 3f, 5, 8 | Actual fear/withdrawal, effective Fight, or patrol defense/peaceful restraint respectively. Setup may bring the scene into reach; actual targets/actions and aftermath decide the verdict. Patrol assignment alone does not prove defense. |
| 4, X1 | Actual receiver, debit/deposit, departure/home outcome and once-only effects. Fund/setup and avatar positioning are flexible; Pay versus Fight and loaded versus unloaded destination remain distinct branches. |
| X2 | Independent rider contact, sight loss/reacquisition and ammunition effects. Avatar teleport is allowed for separation; it does not prove a continuous pursuit path or ordinary walking. |
| X3 | Real original observation/report and distinct newer source knowledge. Teleport between source controls is allowed; do not manufacture discovery or silently credit an unobserved second source. |
| X4/X4b | Controlled casualty or real ambush, surviving actors' continuation, custody and reload. Use each variant's explicitly permitted setup; induced death proves persistence, not combat lethality. |
| X5 | Real unfinished activity, actual interruption and completed recovery or legitimate terminal decision. Bring the interruption into reach; do not replace the worker/activity with a new one and call that recovery. |
| X6 | Connected trip state through scavenge, trouble, return, rest and renewed departure. Teleport incidental transit; keep real interactions and native time/needs when those are measured. No extra walk-home test. |
| E4B | Actual configured model requests and native effects across all selected command routes. Preserve its prepared-fixture, first-attempt and protected-instruction rules; generic setup latitude does not override them. |

Family 6 has its own [phase-specific intent](away-return-journeys.md#scenario-intent-and-setup-latitude).

### Room for surprises, without an expanding finish line

Keep natural encounters in the story when they are compatible with the measured claim. A dog
in bushes can expose a real perception bug without becoming a mandatory dog encounter in every
replay. Observe meaningful surprises, retain their causal evidence and continue the original
outcome when possible. If a surprise destroys a necessary condition, preserve what it proved
and use a compatible continuation or selected branch for the missing claim. Do not erase a real
failure, force a favorable outcome, or add every discovery to every scenario's acceptance rules.
X6 and E4B provide integrated exploration; focused scenarios can also discover surprises without
becoming unbounded exploratory campaigns.

### Combine runs by shared evidence, not by category labels

Reduce duplicate setup where the same actors and native sequence genuinely answer several
claims: 4 + X1 + 6g for payment/return, X3 + 6d/6o for report/source continuity, X4 + 6k–6m
for casualty handover, or X5 within a compatible patrol/companion journey with real local work.
These are opportunities, not required bundles. Credit only exercised conditions. Mutually
exclusive choices need separate compatible branches; faction, channel and caller differences
remain explicit. A focused checkpoint answers its segment, not skipped full-start stages.
No family is removed by this categorization; decide any later reduction from demonstrated
coverage equivalence, rather than from similar names or a single shared component test.

## Coverage, repeat starts and current results

This catalogue describes stable claims and reusable methods; it does not schedule a campaign.
The [owner context](../../../../.de67/owner-context.md), [work ledger](../../../../.de67/work-ledger.md)
and [existing collection results](../../../../.de67/task-logs/harness-approval-simplification-20261009/f9b-gameplay-collection-20261016/current.json)
own current assignments, accepted variants, build scope and remaining work. Inspect the current
judgment/round-stop fields rather than historical counts in that result. Do not restart an accepted
variant or a cancelled round merely to make this document look current.

Both repeat routes matter. A coherent pre-scenario fixture proves the full chain when observed;
a nearest compatible checkpoint makes a focused regression or continuation cheap and proves only
its exercised stages. Ordinary save/reload preserves earlier evidence from the same journey.
Use [selection and launch](selection-and-launch.md) to select either route. Record unavailable
checkpoint bytes as unavailable and use the established recreation recipe; do not invent a save.

Keep successful and failed observations in the existing result with build/run, start/variant,
actual scope and decisive finding. Historical known-good, current verification, regression and
never-proved behavior are different facts. A later failure does not erase an earlier success.
A category is **green for the tested scope** only when every required constituent is supported
and no unresolved defect contradicts it. Otherwise state open, in progress, needs repair, or
blocked with concrete evidence in that existing result. Closing a game is not closing coverage.
Reopen only affected claims after a relevant change or contradiction; unrelated changes and
clerical edits do not reset the suite. Optional exploration does not create an endless obligation.

## Save continuity within faction journeys — 2026-10-08

Use the selected coherent product and complete save for the assigned continuity segment. Save and gracefully exit at a meaningful returning frontier/watch transition. Reload the complete save normally and verify the same living members, physical positions, route/ownership and carried or delivered goods. Continue actual home return/report, then save, exit and reload again to prove effects are not repeated. Cover related abstract progress and both shared faction paths in connected production controls rather than multiplying native scenarios.

Preserve an active failing save only while its diagnosis needs it; do not recreate a consumed historical checkpoint or copy full worlds merely for evidence. Full strict global/world load plus actual NPC/lease reconciliation and native continuation are distinct proof. Recovery is disclosed and idempotent; wrong members, foreign generations, ambiguous provenance, casualties and duplicate items retain refusal. A recovered historical checkpoint alone does not count as a fresh initial journey; recovery within the same observed full-start run retains its established earlier evidence.

Inspect existing complete-save publication and coherent fallback before selecting more code. If publication protection changes, inject failures only into isolated disposable automated saves at real writer boundaries and prove either a complete new generation or coherent prior generation loads without mixed actor/item state. Otherwise report the existing guarantee and demonstrated gap. Record disk/latency implications before choosing storage policy. No new save format, database or fallback behavior is selected here. Preserve sole Luna input ownership; campaign scheduling remains with the current assignment.

Exact scope and research: [save-resilience decision](/Volumes/CodexBulk/Schanigarten/workspaces/Cataclysm-AOL/.de67/task-logs/save-resilience-owner-20261008/scope-and-protocol.md). These are planned acceptance checks, not native launch authority.


## Coverage boundaries and reusable starts

Josef selects four tracks for this expansion: **cannibals, bandits, zombie rider, and local Gemma 4 E4B commands**. Stalker and launcher/clean-install testing are out of this pass; their historical contracts/evidence remain intact and are not deleted or reactivated here. Do not turn the expansion into a general CDDA audit. The thirteen faction arms below remain coverage families. Fresh full-round stories use their specified coherent initial saves; focused completion uses compatible retained state and preserves accepted components. Neither a checkpoint nor production controls prove skipped full-start stages.

Local model behaviour and character scope is **evaluation and report only**; the later owner-selected companion protocol additionally permits repairs of demonstrated runner issues, without changing model instructions or gameplay policy. Josef's handcrafted model instructions must remain unchanged, including actual loaded templates, defaults, snapshot representation and response grammar. Do not tune prompts, sampling/configuration, parsing or gameplay implementation to improve the score during this evaluation. The intended existing local model is `gemma4:e4b`; verify the actual selected model/endpoint when preparing the authorized host. The owner's informal “E4B gemini 4” reference is interpreted as that existing Gemma selection, not permission to substitute a Gemini cloud model. Installation/readiness for using that local model is a preparation dependency, not a reopened launcher certification project. This planning update launches no game and installs no model.

### Scenario index

The ledger owns active assignments, stops and exact current receipts; this index does not dispatch or resume a game. Use the assigned full-start or focused completion route with one named outcome and one native input owner per writable game. Judge encounter behavior, remaining coverage and native closeout separately. Natural attacker defeat can be a valid survival result without proving an unreached discovery, route or contact stage; retain that result while finishing the missing behavior. Indoor raids require their selected contact/combat outcome; payment stories require the actual payer, once-only settlement and physical return, with the resident departure order intact.

The family/variant index below and X1–X6 sections own the concrete starts and outcomes.
E4B uses its [protected companion recipe](llm-companion-journey.md). Choose a story for its
missing claim, not its place in a historical priority list.

The existing signal, night, terrain, contact and away/return variants supply coverage within the
canonical20 families, not an extra practice campaign before them. Preserve the selected full roof
outcome (natural raid, upstairs contact/combat and attributed z1 death), indoor raid outcome, and
both avatar and resident payment branches in their existing families. Resident payment follows
[the current during-watch departure order](bandit-journeys.md#current-resident-payment-run).
Current repair/start decisions belong to the ledger. Use compatible source/build/save bindings,
real host capacity and independent owners; do not make every family a fire-to-raid story or force
an outcome. Stalker and launcher remain excluded.


### Full-stack faction and timing variations — owner direction 2026-10-07

Execute the existing bandit and cannibal journeys through their assigned actual outcomes. Use meaningful variations in signal/contact location, active-watch exposure and local/abstract handoff timing where they exercise distinct implemented paths. Bind each to its real faction schedule and source/run; shared component tests do not establish the other faction's native outcome. Keep payment, peaceful return and release specific to the applicable bandit branch; retain actual raid/combat and survival attribution for cannibals. Use current owner-context scheduling and repair batches; this recipe adds no single-first or fixed parallel-wave gate. Add discovered combinations to existing regression coverage rather than a separate matrix or arbitrary sampling quota.

### Bulky-item robbery payment — owner selected 2026-10-07

For the selected bulky-item payment variant, use recorded physically local M240s before contact; the separate Gold arm retains its own funding recipe. Choose sufficient quantity using actual native Trade valuation; do not invent a demand or force settlement. Complete payment once and verify every selected actual item reaches the bound home stash, preserving contents and quantity, with no remaining payer copy, NPC duplicate, loss or capacity-dependent diversion. NPC carrying capacity does not limit direct stash delivery. Record the initial and final physical item identities and saved counts, then complete ordinary surviving-party home return, release and native closeout. Preserve earlier gold proof and the current held paid-return diagnosis; do not replay the settled payment. Execute when this variant is selected on a coherent ready build with one direct Luna owner; it is not an additional preliminary campaign.

### Indoor bandit active-watch exposure variant — owner selected 2026-10-07

On the coherent repaired build, Luna follows the same indoor initial-save and ordinary-fire journey until actual eligible bandit scouts reach their selected lookout and are actively observing. During that watch, Luna walks outside by ordinary movement into an actually exposed, site-associated position. Record real coordinates/z, time, target-local visibility and any door/window change caused by exit; those changes can affect smoke/light exposure. Maintain current smoke. Check shared physical smoke/light sensing alongside the special watched-tile vision. Record light only when it is genuinely observable. Verify fresh signal and exposed-actor observations, then follow the same surviving carriers home and inspect their actual delivered report and original provenance. Shared signal data does not imply two simultaneous optical callbacks or duplicate reports; retain the actual acquisition path.

Stepping outside does not itself prove visibility. Walls, darkness, invalid observers and unavailable geometry retain their real result. Do not force readiness, relocate scouts, infer equipment or loot, or substitute a refreshed home lead for private watch observations. Preserve the separate closed-window smoke-only arm. Monitor source/run/phase-bound simulation performance under the existing below100ms target, retaining raw alarms and separating tools, modal input, save and load intervals. Mark pure simulation unknown where input exclusion is incomplete. Use the actual watch phase rather than guessed timing or a new wait quota. Native execution follows current repair/build readiness and the assigned arm, without a separate single-first gate. [Variant record](../../../../.de67/task-logs/R-MAINT-SCENARIO-CONSOLIDATION-mac-capacity-001/outdoor-during-bandit-watch-variant.md).


### One E4B companion journey

Use the [complete companion protocol](llm-companion-journey.md) for the single shelter → travel → scavenge → danger → return session: command coverage, character/reliability, pending-response danger, inventory/order interleavings and runner-only repair boundaries. It owns this arm's detailed procedure; do not maintain a second command matrix here. Check actual model/fixture readiness in the current assignment; preserve Josef's loaded instructions and first attempts.

### Cross-cutting safeguards, without extra campaigns

Use the host and frontend selected in the current assignment, with platform deltas explicit; never replace an executable beneath a live run. Share immutable input saves safely, but give each game disjoint writable profile/world/config/cache, session/request/input and evidence identities. One central coordinator selects stories/capacity; Sol oversight delivers each full protocol to one direct Luna game owner and handles diagnosis. Sol does not acquire native input. Two games sharing a local-model server need distinct request/run/NPC correlation; `req_0` alone is not identity. Reconnect must reconcile a possibly still-running request instead of repeating its effects.

Keep performance monitoring during aggressive batching. Combine waits only with unchanged policy and no required decision between them; preserve prompts, danger and alarms. Separate model/cold-start/queue wait, harness collection and host contention from actual simulation turn cost. An alarm whose original trace rotated away has unknown cause; preserve the decisive interval before cleanup. Increase concurrency from observed capacity and useful work, not a fixed RAM cap, while respecting current host availability.

For gameplay fixes, reuse the nearest production caller test and retained failed save; verify the original failure and meaningful negative control rather than asserting only internal flags. Strong cross-cutting invariants are real actor/item conservation, one simulation/input owner, no duplicate transaction/return, evidence retaining original age/identity, and persistence across reload. Short controlled sequences can cover rare stale/retry/failure branches more cheaply than full native journeys. A helper pass is not native proof; a thousand assertions are not a thousand scenarios. No tests or fixes are added merely to satisfy a quota.

Use one existing arm/task result with compact original handles. Classify setup, checker, model and gameplay failures separately. Retest the demonstrated defect and affected claims under the current owner scope; do not resurrect a cancelled round or replay unrelated accepted proof. Keep unresolved dependencies explicit; there is no launcher/stalker inclusion by implication. Clean finished owned processes and disposable fixtures after retaining necessary proof/recovery bytes; no log forest or second coverage database. This expansion is ordered test scope, not new visibility/pursuit/fallback policy.

## Added transition stories — owner-selected 2026-10-05

X1–X6 refer to the six additions discussed with Josef; X4b is the separately selected return-ambush variant, not a replacement for X4; the older faction arm IDs remain unchanged. These are written protocols, not claimed completed native runs. Use standard initial-save recipes for fresh full-round stories and compatible checkpoints for focused completion, recovery or diagnosis. Each brief binds the actual installed copy, immutable build and current source. A recipe name alone does not prove save bytes or platform compatibility. The existing detailed action references supply concrete controls; new debug sequences use advertised menus; promote a reusable correction only under the manual maintenance rule, never invent controls here.

### Shared execution and debug boundaries

Use [live controls](live-operation.md) and [setup interactions](setup-and-interactions.md). Open `play act world.debug_menu` when authorized, then select the actual returned teleport/mutation/kill action and exact target. Debug menu labels and actor IDs are late-bound. Verify the resulting position, trait or death from native state before continuing. Do not use kill-all or move the tested NPC to its expected destination. If the selected scenario blocks the authorized debug action, resolve that specific launch/control capability before starting; do not bypass input ownership or edit the live save.

Josef authorizes practical avatar mutations, including invisibility and avatar teleportation,
across the assigned tests; preserve the measured variable as described in [setup guidance](setup-and-interactions.md#vision-and-other-mutations-by-arm).
X4 separately permits one exact bandit debug kill; X4b permits one hostile zombie rider spawn.
Record meaningful intervention boundaries and verify actual native results. Teleport establishes
spatial/loading setup, not walked travel or NPC return. Invisibility changes perception; induced
death tests subsequent casualty handling, not natural combat damage.

Retain original standardized saves. Run on disposable copies and keep first divergences. Batch unchanged travel/waits aggressively, stopping at relevant contact, ownership, prompt or performance changes. No fixed wait count: wait toward the next evidence-supported transition, and investigate unchanged failure rather than repeating it. Capture source/run/actor/item/operation identity at the relevant boundary, not a full roster every turn. Reuse one report and its native evidence handles; save useful recovery bytes and close owned processes through the normal route. Existing owner STOPs are unaffected.

### X1: Payment across loaded and unloaded home maps

**Start:** a compatible pre-payment bandit visit with accessible local goods and actual receiver. Reuse [resident-payment controls](bandit-journeys.md#ready-report-resident-payment--home-stash). For the resident during-watch story, establish local funding and depart two OMTs during qualified watch, before contact, as the linked recipe specifies. A focused pre-payment checkpoint begins after those stages and does not prove them again. Use the shared avatar setup policy; teleportation establishes location, not walked travel.

**Batches:** (1) establish payer goods, destination home and actual loaded-map bounds; (2) settle one ordinary basket, inspect actual debit and home deposit; (3) visit home if needed, save/reload and verify the same goods once; (4) use a separate compatible pre-payment copy for the other destination-map path. The source distinguishes a home fully inside the current map from a separately loaded tinymap; “nearby” is not sufficient. If existing geometry cannot realize both paths through ordinary setup, report that fixture gap instead of claiming coverage. Do not replay ignition for a pre-choice variant.

Use a discrete-item basket and a meaningful charged-item/container variant where native Trade actually permits them; take advantage of existing grouped-coin evidence. Cancel-before-commit can share the sequence and must leave goods unchanged. Observe container contents and partial charges physically, not just offer value. A failed storage attempt must not be reclassified as a paid visit. Full-storage fault setup is not silently added by this protocol.

**End:** payer decrease matches real home goods/type/count/contents, no extra collector copy or distant-avatar debit, correct ownership, reload preserves exactly one settlement. Separate loaded/unloaded-path results; the existing RLE-aware saved query is authoritative over naïve JSON object counting. Source: `src/npctrade.cpp::complete_trade_to_stash`; current-map versus remote-map insertion and debit ordering motivate the comparison. Reuse old cancellation/charge/return rows rather than duplicate their journeys.

### X2: Two-rider contact and ammunition continuity

**Start:** resolve the existing [two-rider impact](../../../../tools/openclaw_harness/scenarios/zombie_rider.live_native_impact_two_mcw.json) or [band continuity](../../../../tools/openclaw_harness/scenarios/zombie_rider.live_native_band_continuity_mcw.json) recipe to compatible retained bytes and current transport. Record both rider IDs, position/target, ammo and available cooldown evidence. These historical manifests are seeds, not current cross-platform launch promises.

**Batches:** (1) allow real sight and a native ranged/closing action; (2) debug-teleport the avatar behind genuine cover/out of contact, verify endpoint and any actual unload; (3) let native search/forgetting proceed for a relevant interval; (4) teleport the avatar back to a useful reacquisition position, then let the riders physically move and attack; (5) save/reload at a useful encounter or retreat checkpoint and observe continuity. Include one ordinary corner/door break where practical so teleport discontinuity is not mistaken for normal movement. Teleporting adjacent is not a rider closing step.

Watch whether an old impact-ready token can be spent after separation without another qualifying rider step; report the exact sequence rather than inventing a token expiry. Preserve miss/armor outcomes and ensure an impact does not also trigger an unintended extra bite in the same opportunity. Observe the two riders independently, including a naturally differing target/sight opportunity if present. Spend ammunition through real shots; crossing/reload must not refill it. Empty-ammo behavior can be tested if actually reached, without spawning ammo or altering counters.

**End:** same riders, truthful last-known target and sight, independent attacks/recovery, conserved ammo/wounds across loading/reload, no spurious physical-closing credit from avatar teleport. Code questions: `src/monster.cpp::monster::on_move`, `src/mattack_actors.cpp::zombie_rider_impact_actor::call`, `src/zombie_rider_overmap_ai.cpp` observe/relay/persistence and `src/monmove.cpp` action ordering. Existing `tests/zombie_rider_test.cpp` controls are supporting evidence, not replacement native encounters.

### X3: Returning report meets a new source

**Start:** a camp-bearing standard save with real scouts observing source A, preferably just before their return, plus reachable suitable source B geometry. Choose a known viable exposed source; the unresolved closed-room visibility choice is not a prerequisite for this test. Bind A/B coordinates, camp, party and evidence age separately. A second site must genuinely exist; no injected lead/report/knowledge.

**Batches:** (1) retain actual observation of A and departure home; (2) debug-teleport the avatar to extinguish A through ordinary controls and activate a real B source through ordinary controls; (3) verify actual source fields and whichever native emissions/receipts occur; (4) teleport to a useful observation position if needed, inspect actual loading changes, and let the original party physically return/report; (5) inspect the camp's actual decision and preserve/reload a useful checkpoint. Source B discovery is an observed outcome, not guaranteed by lighting it. Avoid accidental contact/burn; record it if it changes the experiment.

**End:** A's report retains correct origin/time/member/revision through delivery; B has its own provenance; reads/reload do not rejuvenate knowledge, overwrite a valid report or apply it twice. A valid power/night/range refusal remains a valid decision, not a mandatory raid failure. If B was never learned, the competing-source case remains unexercised even when A succeeds. Retain the current revision-binding regression and actual source hashes; infer normalization causality only from supporting trace/reproduction. This extends source-off and report-homeward coverage, without repeating all scouting arms.

### X4: Party survivor and casualty handover

**Start:** nearest compatible active bandit pair/outbound or homeward save, with exact member/operation/generation and current body positions. Bind the question before intervention: surviving member continuation and casualty persistence through local/abstract ownership and reload. Existing [6k–6m](away-return-journeys.md#boundary-stress-branches-using-the-nearest-compatible-successful-segment) provide transition queries.

**Batches:** (1) enable and verify avatar debug invisibility to reduce player interference; (2) teleport the avatar to a useful observation/bubble position and record actual loaded membership; (3) debug-kill one precisely identified bandit, preserving the second, and verify actual death/removal/corpse state available; (4) teleport away far enough to unload the relevant area, advance meaningful native time, then return; (5) save/reload at a useful handover and inspect both identities and the operation. Use real owner/actor evidence to establish unloading, not invisibility or distance alone. No NPC teleport, resurrection, mission editing or forced report completion.

**End:** casualty is not recreated or counted home; living actor and appropriate cargo/order persist, one simulation owner, no duplicate party/report/payment. Native survivor continuation or an explained legitimate inability is recorded rather than demanding guaranteed return. Invisibility means this run does not prove ordinary detection, pursuit or fear caused by visible player violence. If visibility is later relevant, explicitly disable/verify it and label that segment. Existing natural combat casualty proof remains distinct; a forced zero-survivor extension is not required.

### X4b: Rider ambush during bandit return

**Purpose:** test whether a real returning bandit group responds to an immediate hostile instead of blindly following its homeward order. This is a separate variant from X4's exact debug kill: the spawn is setup; targeting, combat, wounds, retreat and recovery must happen natively. It does not require a bandit death or victory.

**Start:** a separate copy of a compatible pre-casualty homeward/paid-departure checkpoint with the living group physically loaded. Record each actual bandit ID, body position, HP, equipment, current return goal and operation identity. Reuse the accepted return route rather than repeat payment/scouting. Avatar invisibility and teleportation are permitted for observation; verify their actual state and keep the player from becoming the intended combat target. Invisibility alone does not establish who a creature is targeting.

**Batches:** (1) observe actual native homeward progress before the ambush; (2) through the currently advertised `play act world.debug_menu` spawning controls, spawn **one hostile zombie rider** on a valid unoccupied reachable tile among or immediately beside the group, with clear contact geometry rather than behind a wall/on another floor; (3) verify rider identity/position/hostility and keep the party/rider locally simulated while advancing ordinary turns; (4) inspect each bandit's actual target, attack/reposition/flee action, movement, shots/hits/misses and wounds, plus the rider's target/actions; (5) follow the encounter to threat resolution, escape or a concrete divergence, then observe survivors resume their appropriate return or retain an explained inability; (6) use a useful save/reload or loading transition after that outcome to check persistent survivors/casualties and once-only return. Do not unload the combat immediately after spawning and infer an off-screen fight.

**Expected outcome:** capable bandits with a perceived eligible threat interrupt simple homeward travel to engage/defend themselves. Legitimate native repositioning, fear-driven flight, incapacity or inability to see/reach the enemy is distinguished from a return-order override suppressing combat. Continuing to walk home while knowingly under immediate attack without an appropriate survival response is the specific divergence to retain. No forced target, hostility rewrite, guaranteed hit, smarter-pursuit rule or combat-policy change is authorized by this test.

**Diagnosis and end proof:** first check the actual geometry, loaded actors, hostility, targets, visibility, ammunition and wounds against the intended ambush; then inspect motor/control ownership if that does not explain the result. Source anchor: `src/npcmove.cpp::npc::move` resolves paid-return/withdrawal state alongside danger and combat decisions. Capture the first ignored-threat interval before any recovery; no repeated rider spawning until a desired fight appears. An attack decision alone is not damage, and no damage alone is not refusal to fight. Preserve actual native casualties (distinct from X4's induced death), survivor IDs/orders/cargo, continued homeward progress and no duplicate home/report/payment credit. A legitimate death or retreat is a meaningful result. Use the existing run report and selected actor traces; no extra logging framework or fresh campaign is needed.

### X5: Interrupted work through recovery

**Start:** prepared activity/contact footing with an actual resumable NPC activity and named worker, or nearest compatible checkpoint. Use an activity whose progress/items are visible through existing controls; record progress and materials before the interruption.

**Batches:** (1) let work genuinely progress; (2) permit the selected natural contact or real danger interruption; (3) resolve the current input owner and encounter; (4) observe automatic resumption or issue an ordinary supported resume order as appropriate; (5) follow through to work completion or attributable refusal. Fold into a matching payment or companion journey where the same activity/actor contract is exercised. Use the shared practical setup policy to reach the condition while leaving the actual activity/interruption mechanism intact. An abstract mission narrative is not interruption of a local activity such as ACT_MOVE_LOOT; identify the real active worker and keep that activity unfinished until the interruption occurs.

**End:** no double consumption/output, lost unfinished progress, stranded activity, or inappropriate old-order resurrection after a deliberate replacement order. Distinguish an expected interruption/cancellation from a defect; do not invent automatic-resume policy. A readable demand menu alone does not pass recovery. Use the existing native activity/interruption controls and `doc/npc-ai-interaction-tests.md` to explain the result.

### X6: Longer mixed survival trip

**Start:** useful surviving standard-camp checkpoint, with normal supplies and actual world threats. One coherent trip: leave camp → scavenge → encounter trouble → return and reorganize → sleep/rest → leave again. Reuse observations from X3–X5 where applicable instead of restarting their setup. Keep faction conclusions separate and do not provoke every faction merely to fill the story.

**Batches:** use practical avatar transit, including permitted teleportation, and useful native waits between actual decisions; avatar walking and an ordinary walk-home route are not acceptance conditions; inspect effects at departure, encounter resolution, return and second departure. If testing needs, keep Debug Life Support off from the declared start and supply food/water/rest; if keeping it on to isolate state continuity, label this a continuity trip and claim no needs-survival coverage. Do not opportunistically disable it halfway and call the whole run ordinary survival. Keep real scavenging, encounters, inventory/order changes and rest. Teleportation does not supply elapsed time, satisfy needs or prove a skipped pursuit/exposure segment; retain those conditions only when they are part of the selected claim.

**End:** actors/items/orders/knowledge remain consistent over repeated use, no accumulated duplicates or inexplicable stale orders, save/reload continuity where useful, and performance assessed through the existing configured traces. Stop when the named trip answers its questions; no arbitrary overnight soak or loop quota. Death or an unrecoverable divergence is an honest bounded result with retained evidence, not an erased attempt.

## Existing faction arms and historical coverage

The table retains family identities, compatible methods and source-bound precedent. Historical
success is not a current common-build round; unresolved old descriptions are not live dispatch
instructions. Current results and the ledger record actual completion and remaining work.

| Arm / behavior covered | Reusable route and compatible start | Source-bound precedent / coverage boundary |
| --- | --- | --- |
| 1. Cannibal smoke → scout/report → raid/combat | [Smoke recipe](smoke-raid-replay.md); clean closed-window prepared save, ordinary view. Separate viewer variant below. | R020 ordinary fatal outcome (historical), R027 repaired saved-contact combat (historical). Continuations do not independently prove fresh admission. |
| 1v. Cannibal viewer raid | [Dual-clairvoyance variant](smoke-raid-replay.md#clairvoyance-raid-replay), separate fresh closed-window start. | Historical viewer/control evidence stays separate from ordinary exposure proof; explain every living attacker and defender. |
| 2. Admission → extinguish → routine outings | Shared smoke setup, then [signal-off branch](#signal-off-routine-branch); independent clean pre-signal start, then native admission and extinction. Admitted-roster continuations are diagnostic only. | R018 empty watch/return/report (historical). Populated observations, different POIs and actual haul remain R019 work. |
| 3. Roof smoke | [Roof variant](smoke-raid-replay.md#roof-variant), actual weather/range and source height. | R033 measured party margin (historical); R034 native assessment and report (historical) credited bound smoke, reached certainty5, returned scouts4/5 and delivered report revision1 at minute9000. The five-member camp declined a raid at minute9060 for insufficient party power. R068 full-start roof death (historical) separately proves clean pre-signal ignition through report, natural assault and attributed fatal actor4 damage on old0bdc/74c64. It does not prove an independent light channel or current-build success. |
| 3c. Combined smoke + light | Exposed roof/open-window compatible geometry; verify both channels, then the faction journey. | Smoke-only success does not pass this arm. Verify independent light range/LOS; branch by faction and report each separately. |
| 3r. Rural roof; optional dual-faction variant | [Rural roof protocol](rural-roof-replay.md); offline clean pre-signal countryside fixture A, A+bandit fixture B. | R048 roof arrival/combat (historical) proved IDs5/7 on z1; fatal killer unknown. R050 bandit-only derivative is prepared; R051 (historical) reached smoke lead and a separate terrain outing, blocked before departure. At that historical checkpoint, interior stairs/search and bandit demand were unproved; current disposition is in the linked results. |
| 3f. Panicked raider withdrawal | [Rural panic branch](rural-roof-replay.md#panic-and-withdrawal-regression), real raid contact; one induced fleeing member and calm companion. | Historical plan: escape downstairs, homeward route and return/casualty continuity; roof arrival credited separately. Retain the bandit Fight control at its recorded scope. |
| 4. Bandit smoke → peaceful payment/debt → departure/return | [Bandit branches](bandit-journeys.md), initial bandit save for full discovery/contact proof; compatible pre-choice checkpoints for focused payment/return checks. | R051 run1fe37273 on40bfb2be resumed saved funded/fire state; it proves its recorded continuation, once-only payment, counted custody and both bandits saved home. It is not itself an initial-save replay; bind the earlier clean fixture and ignition segment separately. [Verified segment](bandit-journeys.md#verified-roof-smoke-and-gold-payment-segment). Other signal, receiver, activity and debt branches remain separate; no blanket bandit pass. |
| 4r. Bandit admission → signal off → routine outings | [Bandit routine branch](bandit-journeys.md), own faction start. | Shared controls are reusable; judge bandit-specific travel/observation/cargo/knowledge from its own linked native results. |
| 5. Bandit refusal → effective armed fight | [Bandit branches](bandit-journeys.md), independent initial bandit save through the natural pre-choice sequence. | Actual shots, damage, targets and casualties required; peaceful branch proves no fighting outcome. |
| 6. Leave base → loot → return; exposure and bubble handovers | [Away/return variations 6a–6o](away-return-journeys.md), preferably both factions in the validated rural fixture. | Coverage questions: 24h absences at scout/watch/report/response/payment phases, exposed stalking, pursuit knowledge, split pairs, casualties, reload, vertical crossings and burnout/relight. Preserve independent faction outcomes; current native verdicts belong in the linked results. |
| 7. Lamp light only | Independent electrical/lamp save; replay relevant faction branches with light and no smoke. | Existing R033 lamp-only start; actual staffed light reception/watch be14 retained at limited scope, return/report unfinished. No smoke substitute. |
| 8. Patrol → intruder defense; peaceful restraint | Prepared two-NPC patrol/locker start; see patrol detail below. | Preserve accepted assignment/locker evidence; actual mobilization/combat and paid-shakedown restraint remain separate outcomes. |
| X1. Loaded/unloaded payment-home path | [X1 procedure](#x1-payment-across-loaded-and-unloaded-home-maps) | Real native basket debit/home goods once and reload; prove actual loaded/tinymap path, not distance. |
| X2. Two-rider contact/separation/ammo | [X2 procedure](#x2-two-rider-contact-and-ammunition-continuity) | Authorized avatar teleport separation, actual LOS/unload/reacquisition/shots/ammo/reload; older partials are scoped. |
| X3. Old report / newer source | [X3 procedure](#x3-returning-report-meets-a-new-source) | Preserve original report/provenance/age then grounded new source and native response; no duplicate report. |
| X4. Survivor/casualty handover (incl. X4b) | [X4 procedure](#x4-party-survivor-and-casualty-handover), [X4b](#x4b-rider-ambush-during-bandit-return) | Actual original member/casualty custody, survivor progress/home/report/reload; source corrections do not pass the native arm. |
| X5. Interrupted work / recovery | [X5 procedure](#x5-interrupted-work-through-recovery) | Real ordinary activity/contact interruption and completed continuation. Pulp resumption and natural-stamina cancellation are affected segments, not a whole-family pass. |
| X6. Longer mixed survival trip | [X6 procedure](#x6-longer-mixed-survival-trip) | Whole leave/trouble/return/rest/leave with real survival; old X6 unknown close and historical Casey cause remain protected/unknown. |
| E4B. Companion journey | [Existing companion route](llm-companion-journey.md) | Actual configured model/service readiness and full command trip; preserve owner model instructions, no assumed host readiness or model substitution. |

The assigned constituent claims determine coverage; optional variants do not silently add tests.
Run independent arms concurrently only on isolated writable roots and supported authorized hosts,
with one Luna input owner per game. Keep required Tiles input exclusive and shared model/service
mutations coordinated. Current owner context selects remaining work; this index neither mandates
a whole-suite replay nor reopens accepted claims.

Use the [suite evidence map](playtest-evidence-map.md) to select existing capture and queries, and identify scoped proof gaps before an arm. It covers the away/return subcases too.

## Valid natural playtest deviations — owner correction, 2026-10-07

Random creature encounters may change the planned timing through alarms, combat, flight or other survival responses. Preserve those native behaviors and meaningful threat, health, action and safe-resumption evidence. Continue the same journey when supported; a delay alone is not a bug. Judge a suspected defect against the actual danger and safe alternatives rather than forcing the scripted route. Record the interruption as a valid deviation when the behavior fits the circumstances, while keeping each remaining assigned outcome explicit. A natural survival branch does not automatically prove an unobserved report, payment or raid.

Historical example: in the bulky-M240 pre-payment scouting run1f6c2094, the party alarm stopped renewing, scouts5/6 resumed travel at8130 and physically reached home at8140. The exact stimulus was not recorded, so the nearby bullpup is not attributed as the trigger. Home arrival is positive evidence; typed assessment/report and payment remain separate boundaries.

## Focused regression choices

Current owner context and the ledger own completion scope and repair batches. Preserve
source-bound successes and finish missing behavior from compatible recovery state. A different
journey's late checkpoint cannot prove skipped full-start stages. Query or capture mistakes should
be repaired against available evidence where possible; replay only the behavior still needed.
If a test condition was not reached, adapt the setup and continue. A real defect or external blocker
retains its concrete repair/retest obligation; an unexplained incomplete attempt is not completion.

The following five concrete edge cases refine existing arms; they are not five extra copies of
the same journey or a combinatorial cross-product. Reuse already applicable controls and select
the branch that answers the current regression question. Actual fixture identities and live command bindings belong
in each selected start brief, never guessed from this design table.

For a full-start pass, reach these checkpoints within that journey. Focused completion can use a compatible checkpoint and earns the observed boundary, not skipped earlier stages.

| Existing arm / distinct risk | Start and meaningful interruption | End proof and retained source coverage |
| --- | --- | --- |
| 4 / 6g: Pay, cancel, then pay a grouped basket; reload after settlement | Compatible retained pre-choice resident encounter with recorded local coins and distant avatar. Cancel the first basket before commitment, then pay through actual native Trade; reload only after a confirmed retained save. | Cancel leaves goods unchanged; settlement debits the actual local payer and deposits matching real home-stash goods once; reload does not duplicate them or charge the distant avatar. Reuse available current grouped-coin/charge/save-reload controls or native evidence at its exact scope; the historical stash-conservation raw result was consumed. Historical payment1000 is a comparison baseline, not current full-start proof; do not add a separate preliminary replay. |
| 4: Robbery arrives during an activity | Compatible pre-demand start, begin a supported resumable activity using ordinary controls. Demand/Trade is the interruption, not an artificial test popup. | Readable demand/Trade, correct input owner, preserved unfinished progress/items under the existing interruption contract. Reuse `activity demand keeps native semantic ownership through Pay trade successor` in bandit_live_world_test.cpp and renderer controls; vary activity only for a distinct uncovered lifecycle, not every activity name. Forced incapacity is not ordinary voluntary waiting. |
| 2 / 4r / 6o: Signal disappears after valid learning | A byte-backed completed assessment/report checkpoint; extinguish through ordinary controls, then ordinary departure or continued observation. Keep pre-watch extinction as a separate existing negative example. | Finite knowledge keeps its original age and source, can inform response while valid, and is not renewed by reads/reload. No invented people, inventory or guaranteed raid. Reuse `stationary scout light and smoke share fresh habitation buckets through reload` and prior report-age proof. Closed-room plume semantics remain an owner choice; this case cannot decide it implicitly. |
| 6k / 6m: Pair crosses bubble boundary and reloads | A retained active two-member journey with actual routes; cross the relevant boundary by ordinary travel, retaining the same actors. Reload at a confirmed save with available bytes. | One simulation owner per living member, no duplicate party, route progress preserved, one report/return application. Reuse `homeward reunion retains exact identities and clocks through reload`, `unloaded homeward route persists across actual actor and world reload` and existing boundary controls. Inspect meaningful before/after transitions, not every step. |
| 3f / 5 / 6l: One member flees or is incapacitated during real combat | Compatible actual contact with named party members; use the already authorized panic branch or naturally observed combat injury. For the separate owner-selected debug casualty variant, use X4; its induced death proves persistence, not combat lethality. | Fleeing/capable survivor physically withdraws; incapacitated member is not teleported, falsely returned or resurrected; casualties and damage retain actor attribution. Reuse `paid departure retains each actual survivor flight and forced incapacity` and `active scout flight uses ordinary survival before party return`. One coherent encounter can cover overlapping arm questions. |

The expected outcomes above come from existing contracts. Changing smoke propagation, visibility through walls or forced-incapacity behavior changes the product outcome and needs owner scope reconciliation. Avatar setup follows the shared practical policy; it is not a product redesign. A legitimate capability/night gate is not automatically a defect.
At the first meaningful success or divergence, send the coordinator the outcome, original handle
and remaining uncertainty; finish analysis without withholding established gameplay evidence.

## Select and brief one arm

Use [selection and launch](selection-and-launch.md) for the current registry revision and immutable
build. Historical scenario IDs are lookup seeds, not launch tokens or compatibility guarantees:

- Closed smoke: `cannibal.r_caol_first_smoke_011_ordinary_closed_smoke_assault_r016_clean_recovery`.
- Routine: `cannibal.r_caol_first_smoke_terrain_scout_013`; retained minute8760 save already has
  actors and knowledge, so it cannot prove clean admission by itself.
- Bandit payment: `r032.camp_payment_return_v001_mcw`; recorded locker/ten-coin setup, no injected
  contact. `bandit.live_world_nearby_camp_smoke_mcw` injects fields, not natural ignition.
- Bandit fight: `r032.camp_defense_shakedown_v004_mcw`; verify its actual actor/priority setup.
- Patrol: `r032.camp_defense_zombie_v003_mcw`; preserve its recorded setup and scope.

All arms use the shared [mutation policy and life-support setup](setup-and-interactions.md#mutation-setup-for-suite-runs). Roof and day-away faction runs default to avatar `DEBUG_LS`; ordinary-view and explicit clairvoyance variants stay distinct.

Give Luna one complete arm, intended endpoint, current session/start-save, completed setup,
relevant successful comparison and remaining proof questions. Let Luna choose ordinary strategy
and recovery through that endpoint; do not make each save or elapsed interval wait for the
chaperone. Use the [player/chaperone division](../SKILL.md#live-playtesting-belongs-to-luna)
for shared-resource coordination and genuine blockers. Add `--coordinator-brief FILE` to the selected registry query described in
[selection and launch](selection-and-launch.md); its query input and CLI path remain required. Link the relevant recipe instead of copying the whole suite. Resolve actor/menu
IDs live. Enable supported [NPC decision tracing](evidence-and-diagnostics.md#npc-decisions-during-playtests)
for the relevant actors; disclose unsupported coverage. Inspect meaningful changes, not ritual stops.

A positive raid arm needs a genuinely capable camp. An honest power denial is valid behavior, not
an AI failure or a successful assault. Keep that negative result and select a compatible natural
start, or explicitly record an authorized setup variant. Do not quietly change the roster or margins.

## Signal-off routine branch

Reuse [ignition/wait/actor controls](smoke-raid-replay.md). After natural roster admission,
extinguish via the live Examine choice and verify fire/signal-off; existing remembered knowledge
may persist. Follow same-ID departure → watch or physical visit → return/report, actual cargo and
supply. Observe naturally selected different POIs and knowledge provenance while the player moves.
An empty watch is a valid branch, not haul proof; do not force loot or a target to obtain a pass.
R018's admitted-actor save validates its journey, not fresh ignition/extinguishing.

## Viewer and patrol variants

Arm1 includes a separate [dual-clairvoyance replay](smoke-raid-replay.md#clairvoyance-raid-replay)
when exercising the requested viewer coverage. Ordinary-view combat remains distinct. Reuse the
same controls; record viewing mutations and judge each surviving attacker/defender by actual action.

For patrol/defense, use the owner's next-test priority10 instruction for both actual camp NPCs;
verify duty/shift and completed locker pickup, not just configuration. Observe release from duty,
recruitment and effective intruder combat. Paid peaceful collectors must not provoke unwarranted
defense. Cross-credit only the matching CAMP-DEFENSE/PATROL-SOUND evidence.

## Night alarms and party response

Owner-added 2026-10-02; execution status belongs to current receipts. These are independent FIRST-SMOKE duty/rest arms,
not prerequisites for continuing R051's retained return. Use a compatible disposable save and
immutable repaired build. Luna owns native input. Preserve an unmodified start and record exact
night time, faction/camp/operation identity, actual members, positions, sleep/effects and duty.
Use ordinary avatar vision; do not add clairvoyance to the tested NPCs. Apply the shared SafeOff
and player Debug Life Support setup where needed, declaring any combat consequence; mutations
on the player do not substitute for defender perception. Disclose induced sleep/setup separately
from naturally observed bedtime and do not strip forced incapacity to manufacture a result.

**A. Camp patrol alarm at night.** Use two actual assigned eligible camp NPCs with Patrol10 and
valid patrol terrain/orders. Establish an awake response control and an ordinarily sleeping
eligible off-shift defender. Josef permits a recorded zombie spawn for the threat, or combining
this arm with an actual cannibal raid; keep setup distinct from native alarm/wake/defense proof.
Cause a real awake guard to perceive a hostile and raise the existing
patrol alarm; position the sleeper without direct enemy sight where the alarm can reach them.
Verify alarm origin, reception, sleep ending and actual response movement/defense through the
native scheduler. A diagnostic raised alarm may isolate delivery but does not prove natural
triggering. Observe that the responding NPC does not immediately choose bedtime again. After
threat resolution/expiry, normal duty/rest eligibility returns; do not demand immediate sleep.

**B. Attack a bandit camp at night.** Josef wants to babysit this arm: notify him and coordinate
his participation before launch; do not start it unattended. Use a generated bandit home camp with an awake member and
an ordinary sleeper; retain actual generated membership rather than assigning player-patrol
jobs to make it work. Attack or visibly threaten the awake member so real hostility originates
the alert. Include an occluded but reachable companion. Show alert delivery, waking and each
member's actual defense, approach, regrouping or justified flight. Record attack/damage attribution
and legitimate casualty/incapacity; survival is not a requirement. A sleeping member directly hit
only proves injury waking, so it cannot replace the unhit recipient arm.

**Party extension and controls.** At a suitable existing dispatched-bandit checkpoint, provoke a
real fight involving one member while a reachable companion lacks direct sight; test the same
shared rule for a cannibal party on its own bound state. This does not require restarting fire,
watch and report setup. Keep normal peaceful shakedown/payment non-hostile. Focused production
tests cover unreachable/unrelated members, narcosis/forced incapacity, fear/flight, stale membership
and alarm expiry/reload; use native controls when needed to resolve an actual delivery difference.
Never claim that a player-basecamp result proves generated-camp or dispatched-party behavior.

**Evidence and checkpoints.** Capture before trigger, alarm reception/wake, first meaningful
response and resolution/expiry. Retain source/build/run/turn binding, source and recipient IDs,
sleep/incapacity before/after, membership/order, target knowledge, chosen action, movement and
incoming/outgoing damage. Use existing selected-actor traces; missing observation is not proof
of no action. Diagnose the first material mismatch with the actual caller/motor; avoid repeated
unchanged waits. Save safely and retain the failed arm separately. Return each arm as pass, fail
or inconclusive with its exact evidence handle; these recipes currently provide no native pass.

## Bandit waiting and camp-NPC contact

Owner-required native arms for the consolidated [bandit encounter contract](../../../../doc/npc-bandit-encounter-spec.md). Implementation/source tests do not substitute for these playtests. Use the currently assigned Luna owner and a compatible accepted immutable build; preserve existing completed proof and nearest useful saves. Historical R051/R066 ownership does not authorize a new launch. No competing live input owner or repeated scout setup is needed.

### Waiting outside local simulation, then activation

Use a valid dispatched shakedown and establish that its destination camp/encounter is actually outside the loaded reality bubble but represented by the overmap operation. An adjacent OMT alone does not prove unloading. Record actual loaded-map bounds/active NPC membership and exact party/operation/generation. Advance meaningful native time across an applicable operation update, save and reload while still outside local simulation. The same visit must remain pending without a demand, payment, synthetic combat, refusal from unloaded time or replacement dispatch. Move normally so the camp becomes locally simulated, then observe that the same members/operation resume physical contact once. Verify a real receiver and demand, and that another update/reload does not duplicate the opening or reservation. Preserve actor identities and actual casualties; do not edit ownership/contact flags to force the result.

### Camp NPC receives demand; player on adjacent OMT

Keep an actual basecamp NPC at the target camp and move the avatar onto an adjacent overmap tile. Verify both OMT positions and actual local simulation coverage. Arrange the meaningful distinction: the bandits communicate with the camp NPC, not merely the nearby avatar (retain actual receiver, sight/hearing and contact evidence). If geometry also permits direct avatar contact, move within the adjacent tile or select a compatible setup that separates the receivers; do not count an ambiguous avatar demand as camp-NPC proof. Observe the early threat and camp-response menu identifying the camp/recipient while the avatar remains on its adjacent OMT. Use a saved branch to Pay with reachable camp goods: demonstrate local goods/cargo/value change, distant avatar inventory unchanged and actual peaceful departure. Verify no repeat payment/menu after update or reload. Retain an independent Fight branch when needed by the existing suite: local response, no NPC teleportation, actual attacks/survival and truthful damage evidence. Camp NPCs must not attack or be attacked solely because a peaceful party is present.

For both arms retain native contact/menu/action and saved-state handles, source/build/run identities and before/after operation state. Report missing contact, remaining unloaded state or a behavioral divergence honestly; neither proximity nor a menu-only screenshot establishes the whole outcome. Setup interventions are preparation, not natural gameplay credit. The existing empty-camp/unheard/source controls remain in the contract; these two requested native arms are explicitly pending until actually observed.

## Encounter variations to select when relevant

Owner-requested comparison, 2026-10-03. “Existing” means an arm is already specified, not that it passed. Use retained starts to combine compatible observations; do not multiply every row by every monster, time and terrain. Mark actual build/run results in the existing arm record. New cases below are pending.

| Encounter variation | Bandit coverage | Cannibal projection / remaining native check |
| --- | --- | --- |
| Player encountered outside | Existing arm4 peaceful demand → Pay → goods/departure; arm5 independent Fight. Early-contact behavior remains pending. | Existing arm1/3r: real hostile arrival and effective attack, no demand. R048 roof proof is historical at its scope. |
| Player inside/on another floor | **Add to early-contact arm:** audible indoor shout opens demand; an actually unheard/occluded receiver does not. | Existing roof/interior/6n route; explicitly finish real entry/stairs/search and engagement, not just arrival on target OMT. |
| Camp NPC receives contact; avatar adjacent OMT | Required arm above: actual NPC receiver, remote response, local Pay and independent Fight. | **Add to arm8/1:** raid contacts actual camp defenders while avatar stays adjacent; observe real targets/damage and defender response, no avatar substitution or Pay menu. |
| Empty camp or no reachable receiver | **Add:** local search ends by existing bounded withdrawal; no invented demand/refusal or current-player tracking. | **Add to6f:** raid searches the known site, has no omniscient pursuit of absent avatar, then follows existing search/withdrawal outcome. Record any missing production rule as a finding, not authority to invent loot or auto-battle. |
| Destination outside reality bubble → load | Required pending-wait/save/reload/once-only activation arm above. | Extend existing6e/6f/6m: verify actual raid ownership/progress while unloaded and same-ID local arrival/combat on return; no duplicate actors or fabricated offscreen casualties. Bandit wait behavior must not be silently imposed on raids. Any missing raid destination rule is an explicit outcome/design gap. |
| Player or camp ally attacks before demand | **Add separate pre-contact branch:** actual attack (including an avoided hit where observed) releases self-defense; continue to effective fight. Distinct from selecting Fight. | Existing combat/night attack and party alarms; real player/defender attack must permit defense, but raid is already hostile. |
| Zombie/other threat interrupts encounter | **Add:** meaningful third-party attack before/around demand; bandits defend/flee appropriately without treating it as player refusal or attacking peaceful camp NPCs. Resume demand if survivors remain eligible. | **Add to existing combat arm:** real third-party interruption with actual target selection/survival and subsequent raid continuation or justified retreat. One representative threat suffices unless code shows a distinct capability matters. |
| Sleep, forced incapacity, night alarm | Existing night arms and party extension, still pending at their stated limits. | Existing separate cannibal party alarm/response arm; night raid schedule and survival stay faction-specific. No demand variant. |
| Fear/flight, separation, casualties | Existing3f,6k/6l: individual retreat, survivor continuity, no resurrected/duplicated member. | Existing3f and6k/6l; verify raid-specific withdrawal; one survivor and all-dead branches may reuse observed outcomes. |
| Reload during interaction or after outcome | Existing required pending/menu/payment controls plus6m/6g; no duplicate demand/payment, peaceful departure remains peaceful. | Existing6m/6l: preserve combat/withdrawal/casualty identity; no Pay/debt tests apply. |
| Player breaks sight and moves elsewhere | Existing6j seen/unseen comparison: reported camp is not live player tracking. | Same6j with raid knowledge/perception; preserve separate faction conclusion unless equivalence below is demonstrated. |

### When one faction can cover a shared path

Current source provides partial reuse, not whole-encounter equivalence: `bandit_live_world::is_active_local_assault_member` admits both raid and combat-released shakedown into `active_local_assault_site_for`; `npc::on_attacked`/`raise_faction_alarm` are common callbacks with faction/group context. However `hostile_operation_player_relationship_for` is shakedown-specific, and `active_hostile_withdrawal_site_for` explicitly filters raid. Shared names therefore do not prove equal entry, eligibility or recovery.

A focused source regression may cover both with parameterized real faction inputs. To avoid a duplicate native segment, retain the actual caller → shared helper → outcome path, relevant input/branch equivalence, source/build binding and matching controls for both faction entries. Then credit only that shared segment (for example an identical motor step), not faction dispatch, nighttime gate, parley, target choice or withdrawal. Record the equivalence beside the result; without it, keep the faction-specific native check. No exhaustive cross-product or blanket “same code” exemption.

## Return and tend the suite

Return build/run/start identity, outcome or first concrete divergence, tested action/query corrections,
evidence links and save/input ownership. Keep run results and remaining work in the existing task/ledger. Promote only verified reusable
corrections under the [manual maintenance rule](../GAME-MANUAL.md#add-prune-and-correct-notes), replacing obsolete advice rather than appending a run diary. Reuse shared segments and keep unvalidated faction differences explicit. No extra review gate, mandatory
save schedule or duplicate protocol database. Sol adds macros only for demonstrated repetition.

Keep the configured [performance alarms](live-operation.md): waiting alarms use a 100-turn mean above 100ms, not a per-turn failure rule; non-wait assessment needs its bound configuration. Record missing hooks honestly and separate inference/transport wall time from simulation performance. Preserve first failing checkpoints for exact repair replays.
Do not stop merely at a checkpoint, first harm or a future night gate; follow the intended outcome
or establish the concrete contradiction. Use [witness/finish](witness-and-finish.md) to retain
native proof and safely close or hand over the owned session.
