# Native returned-turn reconciliation candidate

Assignment: `R-CAOL-FIRST-SMOKE-exploration-003`. This is an isolated method candidate; the installed DE67 tree and live deadline database were not changed.

## Original evidence and limit

- The exact smoke worker claim bound task `R-CAOL-FIRST-SMOKE-exploration-001` to native thread `01a0d507-1891-7f30-9cb7-af8e7fe1c658` under coordinator `01a0d4fd-9ed2-7973-9a86-636630250699`. The worker's latest `task_complete` is physical line 9729 of its original rollout, for turn `01a0d5de-114d-7041-aa1f-7e358eb585c2`. This is a returned partial turn, not task completion or whole-claim acceptance.
- Its child `01a0d507-ef3e-7742-935b-d4bd66ef0bc2` has a later `turn_aborted` at physical line 5125. The old runner status files have no recorded CLI PID/birth. The candidate therefore does not claim this historical scene was automatically quiescent. External restart already normalized its attempt; this candidate does not reverse that event.
- The review's [counterexample](../review-first-smoke-20260925/quiescence-counterexample.json) shows five old named registry rows still marked running while `live_task` was false. Their released or terminal ownership, rather than registry text, decides the gate.
- Exact source paths, SHA-256s, line identities and current normalized clock state are in [native-source-audit.json](native-source-audit.json). [candidate-original-replay.json](candidate-original-replay.json) records the candidate's read-only result on those original handles.

## Candidate behavior

`scripts/native_turns.py` selects only nonterminal, unreleased deadline claims. For a native worker, it verifies the exact Codex spawn edge, deterministic task path, workspace and session metadata, then requires the latest turn to have an exact `task_complete`. Every descendant thread is checked recursively. A final answer, open or aborted latest turn, mismatched edge, missing rollout, or changing rollout cannot prove return. A new turn may supersede an earlier incomplete turn in the same thread; its own completion must still be present.

The owning CLI runner now records the transport PID and OS birth identity. Native quiescence requires a completed, zero-exit run bound by `thread.started` to the exact coordinator, plus absence of that PID/birth at decision time. Missing and failed transport evidence stays live. Existing named-worker `returned_assignments` is retained and joined with native results. `workspace_facts`, `mutation_gate`, active coordinator selection and claim-clock retirement share this check. The mutation gate checks raw nonterminal tasks because `deadline_missed` remains an open attempt even though `list_tasks` no longer labels it `running`.

The candidate changes only review routing. It does not terminalize tasks, release worker claims, accept evidence, alter another assignment or allow concurrent reviewer and coordinator work. The supervisor still owns its mutation lock and reviewer launch.

## Verification

- Seven focused native reconciliation tests pass: returned partial work, active descendant, missing/failed/live transport, final text without turn completion, aborted descendant, released historical row, independent active task, and subprocess reviewer handoff. The handoff test confirms the task, claim and checkpoint evidence remain intact.
- Candidate aggregate: 227 tests pass across native, runner, policy, supervisor, worker library and phase-3 scenarios. See [candidate-test.out](candidate-test.out).
- The broader deadline harness suite has one failure that also occurs against the installed baseline: `test_interval_thirty_dfs_requires_ordinary_and_universal_before_restart` reports `Random mutation cycle has no universal component`. This candidate does not change that random-cadence path.
- `candidate.patch` applies cleanly to a temporary copy of the installed method, and all seven patched files match the candidate tree afterward. `candidate-manifest.json` records base and candidate SHA-256s.

First open boundary: promotion requires an exclusive method review. The historical smoke attempt itself lacks the new transport receipt and has an aborted child; it remains preserved under the review's external normalization, with the game proof continuing through its separate named Luna assignment.
