from pathlib import Path
r=Path(__file__).parent;b=r/'baseline';c=r/'candidate'
s=(b/'FS.md').read_text()
def replace(old,new):
 global s
 assert s.count(old)==1,(old[:90],s.count(old))
 s=s.replace(old,new,1)
replace('Status: Refrozen — 2026-09-20','Status: Refrozen — 2026-09-21')
replace('Source baseline: `Cataclysm-AOL | dev | a46200ef7884ae62a8aedd58e27d3f08a5f2d5bb | inspected 2026-09-20; owner WEC addition dirty on entry; unrelated untracked .agents/skills/typesafe-ai/ and skills-lock.json preserved`','Source baseline: `Cataclysm-AOL | dev | fe8c418c1636bdfa6e16d0009af83873a079e24f | inspected 2026-09-21; existing harness/game/test repairs and owner context dirty on entry, preserved in review-owner-f5d913c63d0e/baseline-state.json; unrelated .agents/skills/typesafe-ai/ and skills-lock.json preserved`')
replace('The current performance/travelling addition is specified in S-PERFORMANCE below.', 'The current owner addition is the full-stack harness outcome in S-HARNESS/H-ERGONOMICS below.\nOwner trigger HARNESS-FULL-STACK-20260921 authorizes this exclusive FS thaw/refreeze and directs\nSol to deliver the resulting repairs and native validation. The mutator specifies and validates\ncontext; it does not implement the harness. Preserve all earlier accepted proof and recoverable\nwork. The explicit 2026-09-20 Phase-3 start remains current; earlier phase-only holds are historical.\nOnly the external supervisor launches the one requested post-review coordinator.\n\nThe earlier performance/travelling addition is specified in S-PERFORMANCE below.')
replace('Harness execution and alarms precede acceptance of their native playtests. Phase 3 and gameplay remain stopped until a later explicit owner start.','Harness execution and alarms precede acceptance of their native playtests. The later explicit owner start recorded above authorizes delivery; this historical addition imposes no new stop.')
replace('This refreeze does not restart it or authorize Phase 3. Optional experiments remain deferred and additionally depend on the new harness acceptance.','The earlier refreeze did not restart that campaign; current delivery authority is stated above. Optional experiments remain deferred and additionally depend on the new harness acceptance.')
replace('## Lean, reliable harness — S-HARNESS, owner addition 2026-09-20','## Lean, reliable harness — S-HARNESS, reconciled owner additions 2026-09-20/21')
replace('### Inspected production map and causal diagnosis — H-MAP',(r/'fs-owner-addition.md').read_text()+'### Inspected production map and causal diagnosis — H-MAP')
replace('| Boundary | Current owner and consumers | Observation at source baseline |','The following original ownership map records the earlier 2026-09-20 baseline; H-ERGONOMICS\nand current receipts distinguish repaired behavior from remaining gaps. Do not reimplement an\nalready-working path merely because its historical gap appears here.\n\n| Boundary | Owner and consumers | Observation at earlier source baseline |')
replace('**Mechanism.** Replace the callback-only completion seam in `open_cockpit_game_service` with a\nshared driver used by `CockpitService` operations and `execute_probe_steps`.', '**Mechanism.** Complete and consolidate the existing receipt-bound collector and the\n`open_cockpit_game_service` seam as one transition contract used by `CockpitService` operations and\n`execute_probe_steps`. Preserve the validated raw-wait/keep-watch slice from receipt\n`a665ac048c6de459f5e42fd8858b360dfed1adaf8462e7b06c30f7b8bfb643bc`; it proves normal waits\nand focused controls, not the remaining live interruption or the whole shared-driver contract.')
replace('Submission → accepted input → native activity progress remains **pending**.', 'Submission → accepted input → native activity progress remains **pending**. Operation lifecycle\nand input availability are separate facts: an offered Pause control does not demand its use, and\na menu/owner transition is not completion. Preserve accepted/running/awaiting-decision/completed/\ncancelled/failed/unknown distinctions through existing native types and receipts; no second state\nowner or frame-name exception substitutes for correlation. Native action choice stays with the\nLLM; the driver executes the chosen outcome and declared interruption policy, not a scenario solver.')
replace('Process death or rejected/corrupt binding returns **failed** with the evidence and last known\nprogress.', 'Process death or rejected/corrupt binding returns **failed** with the evidence and last known\nprogress; if already-dispatched native effects cannot be established, their outcome remains\n**unknown**. Never infer no effects or automatically retry merely from transport failure.')
replace('Unsupported durations/actions return an honest supported choice set or missing capability, never\nan invented input.', 'Unsupported durations/actions return an honest supported choice set or missing capability, never\nan invented input. The player chooses duration/target and what to do after interruption; native\nmenus already support longer waits and impose no demonstrated 20-second cap. Use the longest\ncurrently advertised duration compatible with that chosen intent and required observation boundary.\nWall-time collection budgets do not change requested game time. Diagnose ordinary early-pending\nround trips and choose an appropriate bounded internal collection default from observed operation\nbehavior, respecting cancellation and remaining task budget; no arbitrary global sleep or time cap.')
replace('Every response projects operation/request ID, lifecycle status, reason/uncertainty, input availability\nand owner, relevant current game state/progress, valid next operations and evidence handles.', '''One player-facing contract is shared by `gameplay_display`, `cockpit_evidence`, `evidence_display`,
CLI/help, guides, examples and tests. Full observations, deltas and owner changes locate corresponding
facts consistently. Every advertised selector and retrieval command works against the response
actually received, including its displayed versus retained source root. Large action catalogs keep
native navigation and the relevant prompt/choices visible with useful paging; do not render the
entire menu merely because input ownership changed or promise a catalog formatter never called.

Every response projects operation/request ID, lifecycle status, reason/uncertainty, input availability
and owner, relevant current game state/progress, valid next operations and evidence handles.''')
replace('save result, process state and reload readiness when relevant. Compact means selected useful facts,\nnot a hard output quota.', '''save result, process state and reload readiness when relevant. Prioritize what happened, whether
it completed/interrupted/failed/remains pending, game-time delta, changed state, current input owner
and available native responses. Unchanged startup/process/recovery/transport metadata, repeated
manuals and generic retrieval instructions remain in exact receipts and relevant diagnostic views,
not every ordinary successful reply. Internal validation stays intact. Compact means selected useful
facts, not a hard output quota; do not preserve metadata while dropping a consequential choice.''')
replace('Reuse the returned current frame when valid; do not issue a redundant observe to make presentation\nwork.', '''Protect the mandatory decision view before reducing optional detail. Compare changed entities/zones
by stable identity rather than reprinting first-five prefixes of changed whole lists; include a newly
changed sixth NPC ahead of unchanged preview occupants. Use event cursors for messages since the
previous observation and distinguish retained fixture/history from this run's events. Grouping
repeated text must not conceal a new event, actor or chronology. Exact pages must be usable rather
than recursively creating omission handles; count the complete recovery/retrieval chain, not only
initial bytes. A refresh may intentionally replace a baseline but should not be necessary merely to
recover a lost result. Reuse the returned current frame when valid; do not issue a redundant observe to make presentation
work.''')
replace('those\nowners; contradictory authoritative records stay explicit. Preserve serialized submit/collect and','those\nowners; contradictory authoritative records stay explicit. Preserve serialized submit/collect and') if False else None
replace('Preserve serialized submit/collect and\nout-of-band cancellation so two clients cannot race a new action.', 'Preserve serialized mutation/result reconciliation and out-of-band cancellation so two clients\ncannot race a new action. Relevant read-only controls/messages/inspection should remain available\nduring collection using consistent retained snapshots or equivalent safe reads; remove unnecessary\nexclusive-lock contention without publishing mixed generations or racing state writes.')
replace('Tests isolate unsupported platform seams honestly; no same-process claim from restarting an exe.','Tests isolate unsupported platform seams honestly; no same-process claim from restarting an exe.') if False else None
replace('<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-LIFECYCLE-S001 claim=R-HARNESS-LIFECYCLE -->',(r/'fs-new-slices.md').read_text()+'<!-- DE67:DFS-SLICE:BEGIN id=R-HARNESS-LIFECYCLE-S001 claim=R-HARNESS-LIFECYCLE -->')
replace('no fixed run count or full historical campaign.','no fixed run count or full historical campaign.\n\nAlso trace the owner-reported “10 waits / almost 50 commands” from its original run if accessible,\notherwise label a reproduction explicitly. It is not the separately observed 42-action/13-minute\nwait. Classify each step as gameplay choice, necessary native menu interaction, file/schema work,\ntransport, collection, selector repair, redundant observation or evidence retrieval. A source-bound\nlong wait and a materially different long/async operation test common execution, not just two\nconfigurations of the same happy path. Native gameplay interpretation remains separate.')
replace('A fresh agent uses only the documented session\nhandle to recover an interrupted/pending native run;', 'A fresh agent uses only the documented session\nhandle to recover an interrupted/pending native run;') if False else None
replace('Completion requires the five original R-HARNESS claims plus R-HARNESS-PERFORMANCE and their named evidence, a current consumer/deletion inventory\nand a working documented journey.', '''The campaign investigates varied available scenario families: waiting/time-dependent behavior,
movement/navigation, menus/input-owner changes, NPC interaction/async speech, interruptions,
multi-feature interactions, launch/recovery/continuation, and evidence/witness closeout. Inventory
the registry and select representative difficult as well as straightforward cases; repeatedly
passing an easy case is insufficient. Track inspected, attempted, completed, blocked and untested
coverage separately. The coordinator helps the worker investigate valid alternatives without
competing live input. Each rerun states changed premises or the new evidence it seeks, preserving
the previous negative/inconclusive result. A native product failure can be a successful harness
usability result if the failure and its evidence are exposed correctly; it does not close the
separate gameplay claim.

A fresh agent must encounter an unfamiliar situation, choose a native action, receive completion
or interruption, inspect relevant evidence and continue or close through the published interface.
No temporary helper scripts, undocumented selectors, repeated agent polling or reverse-engineered
response shapes may be necessary. Record actual awkward steps; do not smooth them out of the audit.
Static review alone does not satisfy this native usability requirement.

**Campaign deliverable.** Publish (1) the largest verified waste causes; (2) actual branch/commit and
relevant dirty inputs; (3) source/symbol findings, reproductions and observed versus inferred impact;
(4) the classified wait trace or labelled reproduction; (5) coordinator/worker native attempts,
including difficult cases, creative alternatives and blockers; (6) the scenario-family coverage
record; (7) retained confusing/bloated replies and tested decision-complete alternatives; (8) a
prioritized minimal repair list with acceptance evidence; and (9) measurement limits plus old findings
now fixed, obsolete, incorrect or not reproducible. For each improvement identify removed chores,
remaining LLM choices/native checks/evidence, existing partial solution, smallest supported change
and regression/usability test. This is the existing campaign's report, not a new reporting service.

Completion requires the five original R-HARNESS claims, the three H-ERGONOMICS claims
(R-HARNESS-EVIDENCE, R-HARNESS-PREMISES, R-HARNESS-CONTINUATION), R-HARNESS-PERFORMANCE and their
named evidence, a current consumer/deletion inventory and a working documented journey.''')
replace('### Earlier harness refreeze record','''### Full-tree effort and target comparison — H-COST

Use S-EFF-COMPARE for equivalent completed diagnostic/action/resume/closeout outcomes, and compare
combined campaign delivery separately from a single operation. Preserve source/build/scenario state,
claim, information, evidence quality and meaningful gameplay choices. Reuse captured failures and
frozen comparisons when they answer the same question; fresh native runs address changed inputs or
unproved native boundaries. Do not turn an efficiency comparison into a ritual replay campaign.

Where available record actual model requests and uncached input, cached input and output tokens,
separating coordinator, worker, helper, reviewer, provider, handoff, retry and recovery contributions.
Deduplicate own-response records rather than inherited rollups. Report gameplay decisions,
transport-only commands, collection/polling, evidence retrieval, forced full observations,
file/schema construction/repair, displayed bytes by section, elapsed time and completed objectives.
Classify playing, polling, retrieving evidence, diagnosing failure, repairing interface
misunderstandings and reporting/closeout. Command/character/byte counts and existing transcript-derived
“model round-trip” values are proxies, not actual model usage. Missing provider usage stays missing.
Account-wide allowance or estimated weekly pace is not campaign consumption or token billing.

Measure delivered context duplication and unnecessary model decision frequency without reducing the
player's legitimate decisions. Cache/reuse interface knowledge and accepted results. Report both
per-category token changes and total input-plus-output change with cached input visibly separate;
where actual price/cost is unavailable do not equate this total to a bill. The 50% target is a
before/after goal for the same useful outcome, not an invented universal cap or a claim of savings
from fewer CLI calls. Include implementation/audit/review cost separately from steady-state per-run
cost so a benefit is not merely shifted elsewhere. If comparisons or coverage are insufficient,
report that uncertainty and the exact next measurement rather than fabricated savings or claiming
the whole workflow optimal. Target shortfall must remain visible; preserve completed valid repairs.

### Earlier harness refreeze record''')
replace('- [ ] 🔴 R-HARNESS-PERFORMANCE — The shared operation driver reports measured slow turns, spikes, sustained regression and stalled game-time progress without confusing input waits, loading, saving or transport delay with simulation failure.', '- [x] R-HARNESS-PERFORMANCE — The shared operation driver reports measured slow turns, spikes, sustained regression and stalled game-time progress without confusing input waits, loading, saving or transport delay with simulation failure.\n\nDurable acceptance #1 already exists via `R-HARNESS-PERFORMANCE-closure-representative-workload-comparison-001`. This refreeze corrects a stale status projection, not a new acceptance. Its ceiling is instrumentation and representative measurement; gameplay claims remain open.')
replace('**Current gap and boundary.** `process_performance.py::ProcessPerformance`', '**Original inspected gap and boundary (now covered at the accepted ceiling above).** `process_performance.py::ProcessPerformance`')
replace('### Current freeze record — performance addition','### Historical freeze record — performance addition')
s+='''\n\n## Current freeze record — full-stack harness owner mutation, 2026-09-21\n\n- Status: Refrozen from the owner-authorized thaw in gate `f5d913c63d0e`, invocation\n  `mutation-48bcebae6cd84750aecc249a942956f5`, against `dev@fe8c418c1636bdfa6e16d0009af83873a079e24f`\n  plus preserved dirty source/test/fixture repairs. The reviewer changes FS/context, not product code.\n- The full owner brief and prior audit remain lossless under\n  `.de67/task-logs/owner-harness-forcing-mutation-20260921/`; H-ERGONOMICS maps all eleven areas.\n  Review source checks/reproductions, accounting, scope validation and restart receipt are under\n  `.de67/task-logs/review-owner-f5d913c63d0e/`. Synthetic findings are not native proof or savings.\n- Preserve all 64 durable acceptance rows and all old slice/claim identities. Three new claims\n  carry new work; expanded open harness contracts receive no inherited whole-claim acceptance.\n  R-HARNESS-PERFORMANCE's stale red display is reconciled to existing acceptance only.\n- Resume the returned `R-HARNESS-EXECUTION-exploration-003` frontier through the coordinator's\n  ordinary ingress/continuation, retaining useful fixture repairs and its exact result. Source\n  capture/OCR is not a prerequisite for a nonvisual semantic claim; actual startup readiness is.\n  Sol sequences that live-decision proof with the new session/evidence/premise/continuation work.\n  No new game, worker or coordinator was launched by this mutator.\n'''
(c/'FS.md').write_text(s)
print('FS candidate lines',len(s.splitlines()))
