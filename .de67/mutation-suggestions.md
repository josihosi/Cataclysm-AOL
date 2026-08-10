# DE-67 mutation suggestion ledger

This ledger is append-only. Independent reviews and manual suggestions use ordinary Markdown, not a
fixed data schema. Every incident entry must make its short verdict easy to scan; only the current
incident requires its full diagnosis to be reread, while all earlier short verdicts are reread.

Suggested content for an incident:

- incident/task and deadline evidence;
- **Short verdict:** a compact causal label;
- **Diagnosis:** one paragraph naming the first contradicted premise and direct evidence;
- **Suggested mutation:** the guideline section and proposed change;
- disposition after coordinator review: applied, superseded, rejected, or pending, with reason.

Manual suggestions clearly say `Source: manual`. They receive consideration in the next mutation
round but no automatic authority.

A worker-finding entry says `Source: worker finding` and records expected versus observed behavior,
direct evidence, the coordinator's causal classification, and its disposition. For a DFS expansion,
also name the first contradicted DFS premise, the added red IDs, the changed mechanism/ownership/proof
sections, and why the change preserves the WEC, project language, permissions, and acceptance
strength. A worker finding is not automatically a specification gap.

## Entries

### Manual suggestion — overdefined or fragile test protocol

Source: manual

Submitted by Josef.

**Short verdict:** test overdefined / fragile protocol

Incidental dates, flags, or receipt details must not reject product evidence unless they can change
identity, the T01 verdict, or a false-green control.

Disposition: pending the next mutation round.

### Manual suggestion — missing movement observability

Source: manual

Submitted by Josef.

**Short verdict:** tooling missing

Repeated inability to observe overmap AI movement calls for the smallest useful logging or probe
capability, not identical failing runs.

Disposition: pending the next mutation round.

### Manual suggestion — coordination ownership lost

Source: manual

Submitted by Josef.

**Short verdict:** coordination ownership lost

Reading and rewriting handovers is not progress. One coordinator owns the next causal decision, and
another xhigh review requires genuinely new evidence.

Disposition: pending the next mutation round.

### Manual suggestion — on-time findings bypass mutation cadence

Source: manual

Submitted during the T01 acceptance watch.

**Short verdict:** worker-finding lane became a mutation escape hatch

**Diagnosis:** `T01-M1` through `T01-M4` all ended on time with `R-001` still red and zero miss
units. The findings were not identical noise—two produced bounded product checkpoints and each named
a changed causal seam—but the deadline-only mutation cadence therefore cannot fire even after four
unresolved attempts on the same claim. A coordinator can remain permanently busy and apparently
healthy by converting every incomplete task into an on-time finding.

**Suggested mutation:** Add a progress-stagnation review independent of deadline misses. A worker
finding remains useful only when the coordinator names the contradicted premise, preserves any
accepted frontier, and changes the next causal route. Repeated findings on one red claim without an
accepted DFS transition must not authorize an unlimited retry loop; use an owner-supplied run fuse
when one exists, and otherwise require a mutation review before repeating the same route. Do not
turn incidental metadata or longer handoffs into the progress test.

Disposition: rejected by the owner for this run. On-time findings that change the causal frontier
remain acceptable; add no fixed worker-run mutation cadence. Preserve this entry only as a warning
against repeating an unchanged route.

### T01-M1 — exact-head binary gate stopped the natural probe

Source: worker finding

Deadline evidence: `T01-M1` reported an on-time `blocker` finding before its
2026-08-10 20:33:27 CEST deadline; cumulative misses remained zero.

**Short verdict:** tooling unchecked / stale executable identity

**Diagnosis:** The first contradicted premise was that the runnable Mac executable represented the
committed preflight HEAD. The unchanged scenario reached a green gameplay HUD but the harness
correctly stopped before feature steps because the window identified `56fb35f144-dirty` while the
repository was at docs-only preflight `720e24a00a`. Source/history review found that current
`56fb35f144` already follows the jointly safe-pair gate and inclusive loaded-edge route fixes, and
the focused owner/cohesion and inclusive-edge tests passed. The missing fact is therefore natural
current-executable evidence, not a newly proved product defect or same-contract DFS gap. Direct
evidence: `build_logs/de67/T01-M1/natural-probe.log` and
`.userdata/dev-harness/harness_runs/20260810_200712/startup.result.json`.

**Suggested mutation:** No guideline mutation is due without a deadline incident. Apply the existing
Tooling check guidance by producing a clean exact-HEAD Mac executable before the next natural probe.

Disposition: applied as a tooling/evidence classification; keep `R-001` red, do not expand the DFS,
and retry only under a fresh task identity after changing executable identity.

### T01-M2 — natural homeward motor split the exact pair

Source: worker finding

Deadline evidence: `T01-M2` reported an on-time `unexpected` finding before its
2026-08-10 20:29:43 CEST deadline; cumulative misses remained zero.

**Short verdict:** competing per-member movement escaped pair cohesion

**Diagnosis:** The first contradicted premise was that releasing the assembled pair to the homeward
motor preserves coherent paired travel until the atomic boundary crossing. On clean exact binary
`4abe539461+SDL3`, the unchanged scenario naturally dispatched members 4/5, crossed outbound,
dematerialized, and rematerialized the same return at `(164,34,0)`. The selector then admitted a
jointly reachable boundary pair, but the ordinary per-member motor moved member 4 roughly 119 map
squares to its departure while member 5 remained at its original return staging square. Current
source explicitly treats assembly as a one-time gate and retains `cohesion_assembled` throughout
homeward travel, so the cohesion planner issues no regroup order after the split. The atomic commit
correctly refused to fire because both members were not at their departures. This is an
implementation gap under the existing DFS ownership/cohesion contract, not a DFS specification gap.
Direct evidence: `build_logs/de67/T01-M2/return-owner.selector.log` and
`.userdata/dev-harness/harness_runs/20260810_201057/probe.report.json`.

**Suggested mutation:** No guideline mutation is due without a deadline incident. Replan the product
owner so the exact pair cannot make unbounded independent homeward progress before its atomic
boundary transition, and add a focused control reproducing the asymmetric-arrival route.

Disposition: applied as an implementation classification; keep `R-001` red, preserve the unchanged
scenario and identities, and require changed source plus the focused asymmetric-arrival control
before another natural probe.

### T01-M3 — homeward abstract resume never reacquired the loaded owner

Source: worker finding

Deadline evidence: `T01-M3` reported an on-time `unexpected` finding before its
2026-08-10 20:54:05 CEST deadline; cumulative misses remained zero.

**Short verdict:** homeward materialization/recenter gate remained unresolved

**Diagnosis:** The first contradicted premise was that the unchanged initial window would hand the
assessed exact pair back from abstract return to a loaded homeward owner after the new bounded-pair
motor passed focused proof. Exact commit `0d082eda34` naturally completed the observing handoff,
paired ingress crossing, and observing dematerialization, then entered `returning_report` and
`returning_home`; however every remaining production attempt rejected materialization with `loaded
bubble lacks paired entry or staging positions`. The same rejection preceded the later successful
handoff in T01-M2, so the new cohesion patch is not source evidence of an observing regression.
Current source can scan a later route/recenter candidate only behind resume-state predicates, while
the terminal rejection does not emit those predicates or the initial entry/staging counts. This is
an implementation plus tooling gap already covered by the DFS recentered-transfer proof route, not
a new DFS mechanism. Direct evidence: `build_logs/de67/T01-M3/live-owner.selector.log` and
`.userdata/dev-harness/harness_runs/20260810_203544/probe.report.json`.

**Suggested mutation:** No guideline mutation is due without a deadline incident. Preserve the
focused bounded-pair candidate, add the smallest materialization discriminator for resume flags and
pair counts, and fix the normal assessed-return recenter gate only if those source-grounded facts
uniquely identify it before rerunning the unchanged scenario.

Disposition: applied as an implementation/tooling classification; keep `R-001` red and retain
`0d082eda34` as a coherent checkpoint because it closes the independently reproduced pair-split
mechanism without claiming the natural T01 verdict.

### T01-M4 — return resume was consumed and rebound vehicle state contradicted the map

Source: worker finding

Deadline evidence: `T01-M4` reported an on-time `unexpected` finding before its
2026-08-10 21:18:22 CEST deadline; cumulative misses remained zero.

**Short verdict:** handoff adapter dropped resume and preserved stale local vehicle state

**Diagnosis:** The first contradicted premise was that the local/abstract handoff adapter preserves
all physical state needed by the next loaded return owner. Checkpoint `7495ec5286` proved the
existing recenter search when its predicates are present. In the unchanged run, the discriminator
showed the observing dematerialization's resume had been cleared before `returning_report`; current
source excludes `observing` from `consume_local_pair_resume_receipt` retention even though observing
is the phase that just produced the physical resume. A later ordinary homeward handoff still bound
the exact pair, but `spawn_at_precise` preserved an NPC `in_vehicle` flag at an entry tile with no
vehicle; the first bounded motor step called `map::unboard_vehicle` and raised `vehicle not found`.
Both facts belong to the same production handoff adapter and are implementation gaps under the
existing ownership contract, not new DFS mechanisms. Direct evidence:
`build_logs/de67/T01-M4/live-owner.selector.log`, the backtrace in
`.userdata/dev-harness/harness_runs/20260810_205614/probe.artifacts.log`, and current
`consume_local_pair_resume_receipt` / materialization bind code.

**Suggested mutation:** No deadline-guideline mutation is due. Preserve the bounded-pair and
discriminator checkpoints; retain the observing-produced physical resume until a homeward consumer
can use it, and make the bind adapter reconcile passenger flags with the actual loaded entry tile
before the ordinary NPC motor runs. Prove both with focused controls before one unchanged live run.

Disposition: applied as a changed causal route; keep `R-001` red and do not expand the DFS. The new
manual stagnation suggestion is acknowledged: another dispatch is admissible only because it changes
the exact handoff owner and test surface rather than repeating the prior implementation/live route.

### T01-M5 — owner rejected vehicle-support scope

Source: worker finding

Deadline evidence: `T01-M5` reported an on-time `unexpected` finding before its
2026-08-10 21:40:03 CEST deadline; cumulative misses remained zero.

**Short verdict:** owner scope correction / scouts are on foot

**Diagnosis:** The first contradicted premise was that the stale passenger-bit failure authorized
general vehicle-state reconciliation or preservation. Josef clarified that T01 scouts are on foot;
`test_rv`, actual-boarded behavior, boardable-vehicle lookup, vehicle fixture/routing, and backup
preservation are outside the claim. The dirty candidate was reinspected and all such work was
removed. The remaining three-file diff contains only the source-grounded T01 slice: retain the
observing-produced resume, clear `in_vehicle` and `controlling_vehicle` unconditionally after the
bandit abstract-to-loaded spawn, and assert the no-vehicle result in the existing owning handoff
test. The immutable finding terminal arrived before tests or commit, so this corrected diff is not
yet accepted evidence.

**Suggested mutation:** No deadline-guideline mutation is due. Keep worker briefs owner-specific:
state that the scouts are on foot and test only the absence of stale passenger state at this handoff.

Disposition: applied. The vehicle-support branch was deleted before any checkpoint; preserve the
prior accepted checkpoints and continue only with the corrected on-foot diff under a fresh task
identity. `R-001` remains red and the DFS does not expand.

### T01-M6 — observing fixture contradicted the persisted outing invariant

Source: worker finding and fresh-coordinator reinspection

Deadline evidence: `T01-M6` received an on-time orphaned-dispatch blocker terminal before its
2026-08-10 21:49:08 CEST deadline; cumulative misses remained zero. The worker's subsequent focused
result is supplemental causal evidence and does not rewrite that immutable terminal.

**Short verdict:** test setup did not establish a valid observing outing

**Diagnosis:** The first contradicted premise was that changing only `phase` and the handoff phase
from `outbound` to `observing` creates a valid persisted observing fixture. The owning test then
round-tripped waypoint zero and `local_contact_minutes=-1`; current production validation requires
an observing structural outing to be at its destination waypoint with contact time no earlier than
its start. The focused test therefore correctly rejected the fixture after 1,577 of 1,578
assertions passed. This is a test-setup gap, not a source or DFS gap. Direct evidence:
`build_logs/de67/T01-M6/focused-owner-test.log` and `structural_phase_is_consistent` in
`src/bandit_live_world.cpp`.

**Suggested mutation:** No deadline-guideline mutation is due. Use the existing canonical route and
timing fields to form the smallest valid observing fixture, keep the corrected on-foot source diff,
and rerun only the owning test before an unchanged natural probe.

Disposition: applied as a changed test route. Preserve the refrozen on-foot DFS, do not broaden the
test surface, and retry under `T01-M7`; `R-001` remains red.

### T01-M7 — phase-only observing control again failed persisted-owner validation

Source: worker finding

Deadline evidence: `T01-M7` reported an on-time `unexpected` finding before its
2026-08-10 21:55:01 CEST deadline; cumulative misses remained zero.

**Short verdict:** focused test setup contradicted persisted owner invariants

**Diagnosis:** The first contradicted premise was unchanged from the supplemental M6 test evidence:
changing only the outing and snapshot phase from outbound to observing does not create a valid
observing resume. The test left the structural outing at waypoint zero without recorded local
contact, so save/load correctly rejected it before resume retention could be proved. Source review
confirms the three-file product slice remains limited to observing-resume retention and unconditional
on-foot flag clearing. Direct evidence: `build_logs/de67/T01-M7/focused-owner-test.log` and
`structural_phase_is_consistent` in `src/bandit_live_world.cpp`.

**Suggested mutation:** No deadline-guideline mutation is due. Replace the phase-only setup with a
consistent observing abstract resume through the existing owner seam, then rerun the same focused
test before checkpoint/build/live proof.

Disposition: applied as a test-definition gap; keep `R-001` red, do not expand the DFS, and preserve
the corrected product slice.

### T01-M8 — phase-only homeward consumer was rejected

Source: worker finding

Deadline evidence: `T01-M8` reported an on-time `unexpected` finding before its
2026-08-10 21:59:38 CEST deadline; cumulative misses remained zero.

**Short verdict:** focused homeward consumer bypassed its structural owner transition

**Diagnosis:** The valid waypoint/contact correction made the observing resume persist, closing the
M7 test contradiction. The test then changed only outing/snapshot phase to `returning_report` and
called the handoff consumer. Planning could form a snapshot, but commit rejected the state under the
owner consistency checks because the structural assessment transition had not produced it. This is
a second focused-test setup gap, not a product or DFS gap. Direct evidence:
`build_logs/de67/T01-M8/focused-owner-test.log` and `commit_local_pair_handoff` in
`src/bandit_live_world.cpp`.

**Suggested mutation:** No deadline-guideline mutation is due. Produce `returning_report` through
the existing structural assessment/return owner seam, then verify that consumer uses and clears the
retained resume.

Disposition: applied as a changed test route; preserve the product slice and keep `R-001` red.

### T01-M9 — direct tests make omitted the parent Mac build contract

Source: worker finding

Deadline evidence: `T01-M9` reported an on-time `blocker` before its 2026-08-10 22:02:34 CEST
deadline; cumulative misses remained zero.

**Short verdict:** tooling command bypassed repository C++17/Mac flags

**Diagnosis:** The changed test now produces the consumer through the existing schema-10 assessment
transition, but the worker invoked the tests sub-Makefile directly without the flags exported by the
root Makefile. Compilation failed on C++17 library features before the owning test ran. While
recording the finding, backticks inside shell-quoted evidence expanded and repeated the same bad
command once; the immutable evidence is noisy, but no source changed and no product verdict was
claimed. Direct evidence: `build_logs/de67/T01-M9/focused-build.log` and the root/test Makefiles.

**Suggested mutation:** No deadline-guideline mutation is due. Invoke the focused target from the
root with the Mac variables used by `just_build_macos.sh`, then run the owning test.

Disposition: applied as a tooling gap; keep `R-001` red and preserve the changed test candidate.

### T01-M10 — file target bypassed the tests dependency graph

Source: worker finding

Deadline evidence: `T01-M10` reported an on-time `blocker` before its 2026-08-10 22:08:32 CEST
deadline; cumulative misses remained zero.

**Short verdict:** wrong root make target produced a stale no-op

**Diagnosis:** The Mac variables were correct, but the command named the existing file
`tests/cata_test`. The root Makefile has no dependency rule for that path, so make returned success
without invoking the tests sub-Makefile even though `tests/bandit_live_world_test.cpp` was newer than
its object and binary. The dependency-aware root target is the phony `tests` rule, which exports the
root flags and invokes the sub-Makefile. Direct evidence: `build_logs/de67/T01-M10/focused-build.log`,
file mtimes, and `Makefile:1686`.

**Suggested mutation:** No deadline-guideline mutation is due. Use the same Mac variables with root
target `tests`, then run the owning test only.

Disposition: applied as a changed tooling route; keep `R-001` red.

### T01-M11 — focused checkpoint accepted; tracked ledgers blocked exact identity

Source: worker finding

Deadline evidence: `T01-M11` reported an on-time `blocker` before its 2026-08-10 22:10:25 CEST
deadline; cumulative misses remained zero.

**Short verdict:** accepted focused frontier / coordinator state blocked exact binary

**Diagnosis:** The dependency-aware Mac test build rebuilt the changed object and test binary, and
the owning handoff test passed all 1,583 assertions. The product source and real-owner test are
checkpointed at `8d586632a3` and `08470fa60f`. The next exact game build correctly stopped because
the coordinator-owned tracked ledgers were still dirty, which would embed `08470fa60f-dirty` rather
than an exact committed identity. Direct evidence: `build_logs/de67/T01-M11/focused-build.log`,
`build_logs/de67/T01-M11/focused-owner-test.log`, and Git status.

**Suggested mutation:** No deadline-guideline mutation is due. Commit the compact coordinator state,
then build that exact committed Mac HEAD and run the unchanged natural route once.

Disposition: applied as an accepted focused checkpoint plus tooling gate; keep `R-001` red pending
the integrated production chain.

### T01-M12 — required raw logs dirtied the embedded version

Source: worker finding

Deadline evidence: `T01-M12` reported an on-time `blocker` before its 2026-08-10 22:13:10 CEST
deadline; cumulative misses remained zero.

**Short verdict:** required untracked evidence contaminated exact binary identity

**Diagnosis:** The Mac game build succeeded from clean tracked HEAD `5553b42773`, but the required
untracked `build_logs/de67/` directory was visible to version generation and the executable reported
`5553b42773-dirty+SDL3`. The binary therefore could not enter the natural proof route. This is a
tooling/identity gap; no product behavior was exercised. Direct evidence:
`build_logs/de67/T01-M12/exact-build.log`, the T01-M12 identity log, and Git status.

**Suggested mutation:** No deadline-guideline mutation is due. Preserve the raw evidence while
locally excluding only `build_logs/de67/` from Git status, then rebuild the exact committed HEAD.

Disposition: applied as a changed tooling route; keep `R-001` red.

### T01-M13 — inactive in-bounds camp arrival escaped the pair receipt

Source: worker production result and coordinator repetition stop

Deadline evidence: `T01-M13` received an on-time `blocker` terminal before its
2026-08-10 22:15:34 CEST deadline; cumulative misses remained zero. The terminal stopped a second
unchanged natural run and classifies the first exact post-fix run as the causal evidence.

**Short verdict:** camp arrival straddled the loaded/unloaded receipt adapter

**Diagnosis:** The first contradicted premise was that ordinary overmap travel hands every complete
physical camp arrival to the transactional dematerialization owner. Exact run `20260810_213549`
proved members 4/5 crossed and dematerialized outbound, rematerialized in `returning_report` without
the stale vehicle crash, and committed the paired homeward boundary. At the next overmap movement,
both persisted NPCs reached camp OMT `(164,39,0)`, but one inactive NPC's precise position was still
inside the loaded map. `dematerialize_live_bandit_structural_handoffs` accepts an unloaded member
only when it is also out of bounds, and accepts an in-bounds camp arrival only when active. The
overmap loop's reached-destination hold does not set `local_pair_needs_reload` for this inactive
in-bounds arrival, so the pair remains local-owned with no admissible complete snapshot. Direct
evidence: `.userdata/dev-harness/harness_runs/20260810_213549/probe.feature_debug.log` at the
homeward-boundary commit and subsequent motor diagnostics, plus the dematerialization preflight and
reached-owned-destination branches in `src/do_turn.cpp`.

**Suggested mutation:** No deadline-guideline mutation is due. At the existing overmap owner seam,
request reload for an inactive in-bounds owned camp arrival before the second dematerialization
opportunity, and add one focused asymmetric loaded/unloaded camp-arrival control. Do not change
scenario geometry, identities, timing, vehicle behavior, or the pair-transaction contract.

Disposition: applied as a changed implementation route. The duplicate natural run was terminated;
preserve the owner-corrected on-foot checkpoints and retry only after the camp-arrival adapter and
focused control change. `R-001` remains red.
