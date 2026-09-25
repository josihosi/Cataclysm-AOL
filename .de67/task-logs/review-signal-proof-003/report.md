# Signal proof incident review — 2026-09-20

Invocation `mutation-6962eea293914e118c52f7f296badaec`, lineage `semantic-surface-cockpit`, incident task `R-CAOL-SIGNAL-PROOF-exploration-003`, claim generation 1. Astra owns synthesis; one bounded Luna helper traced the build handoff. No coordinator, roster worker or game was launched.

## Disposition and diagnosis

The complete `.de67/mutation-suggestions.md` pending section was read and is empty. There are no owner entries to consume or preserve as blocked. Historical reviews were not treated as current requests. The current marked owner contract authorizes Phase 3; older ledger holds are explicitly historical.

The three-hour claim deadline elapsed with valid partial repair work but no complete signal/camp proof. The final attempt repaired the focused helper phase and compiled it, then failed to execute the corrected behavior tests. This is a real missed outcome, not a claim that all prior investigation was waste. Exploration-002 found a native local-owner sound-retention contradiction and repaired source/fixture issues; its receipt retains that ceiling. The deadline and missed generation are preserved.

The strongest preventable friction is a missing executable build handoff. The exploration-003 packet carried a whole-claim fallback rather than an explicit task-sized ledger assignment. Its retrieved predecessor receipt described a headless focused failure without the runnable build recipe. The source log still contained the working root-Makefile command. The successor searched build paths, tried incomplete direct sub-Makefile invocations, then manually linked root game objects into the test executable. The game entrypoint was selected; compilation/link success could not establish behavior execution.

Source handles:

- Packet `.de67/state/worker-dispatch/task_522d43414f4c2d5349474e414c2d50524f4f462d6578706c6f726174696f6e2d303033-b3298c19ea913d318cf98f46020fe2ceb87aaffa971671858f223c29a4021e0f.md`: line 1 whole-claim fallback; lines 34–36 exact context retrieval available; line 45 permits asking the coordinator about stalled progress. Packet SHA equals its filename binding.
- `/Users/josefhorvath/.codex/sessions/2026/09/20/rollout-2026-09-20T19-33-02-01a0bfe0-e862-75c2-b4f8-7644d31d6e35.jsonl`: line 3435 original root build command; line 3614 focused invocation. The predecessor test executed but failed three fixture assertions (46/49 passed). It was not a passing binary to accept unchanged.
- `/Users/josefhorvath/.codex/sessions/2026/09/20/rollout-2026-09-20T21-42-35-01a0c057-825b-7773-8a2a-a939712a57ae.jsonl`: line 19 confirms packet read; lines 268–306 direct sub-Makefile attempts; lines 319–374 manual root-object link attempts; line 381 attempted filter; line 430 acknowledges game startup and no test credit.
- `tests/Makefile:70` links test objects against `../$(BUILD_PREFIX)cataclysm.a`. Root `r033-headless-obj/*.o` is the game object set, not the `tests/` object set.
- `.de67/task-logs/R-CAOL-SIGNAL-PROOF-exploration-002-receipt.json` and `...-003-receipt.json` retain native contradiction, partial work and limitations.

Counterevidence: the existing coordinator handoff contract already permits reusable selected context and useful worker reuse, and the packet permits targeted retrieval and help. No inspected instruction required manual linking, forbade the supported command or required rediscovery. Missing recipe is a supported contributing cause, not proof that better prose guarantees agent execution or that the whole deadline miss had one cause. The helper's suggestion that the no-gameplay statement was self-imposed is not needed for this finding and is not adopted as a causal conclusion.

## Correction and falsification

Immediate recovery: preserved hashes of the incorrectly linked binary and directly compiled test object, removed those generated outputs from the active build, then invoked the predecessor's supported root command:

```
make -j4 BUILD_PREFIX=r033-headless- SOUND=1 LOCALIZE=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 TESTS=0 tests
tests/r033-headless-cata_test '[phase4_sound_observation]' --reporter compact --user-dir .de67/task-logs/review-signal-proof-003/test-user
```

`headless-build.json` records exit 0 in 39.26 seconds. `focused-test.json` records Catch2 execution in 12.49 seconds, exit 2. The complete `focused-test.log` exposes two assertions in one test case: line 15, source line 18217, `last_progress_minutes` 102 versus expected 101; lines 16–17, source line 18241, round-trip rejects an inconsistent active outing owner. This falsifies the earlier assumption that correcting phase alone settled the fixture/behavior boundary. It also proves the recovered route reaches behavioral checks rather than the game entrypoint. No product source was changed by this review and no focused pass, native repair, save/reload success or whole-claim acceptance is claimed.

Repeatable correction: current work ledger now gives Sol a task-specific continuation with the exact build recipe, fresh contradiction log, source invariant question, and preserved broader native obligations. The recipe is inside the assignment body, so existing `exploration_assignment()` extraction retains it. `handoff-validation.json` records the real parser result and assignment digest; `mutation_guard.py work-ledger` passed with seven active items. This is a tested reusable result and improved information delivery, not a new universal rule or tooling framework. All independent assignments, accepted ceilings, source work and FS slice identities remain intact.

Broader method disposition: guarded no-change. Evidence does not justify rewriting policy or installed guidance; those already support the desired reuse. Therefore no policy candidate or method candidate is promoted, and no random lane is invented for this incident review. The supported local recovery and context correction are validated; future successor delivery/use and end-to-end savings remain unobserved. Sol owns ordinary product follow-through under the preserved claim, including whether the fresh assertions expose malformed fixture state or a production recorder fault. Do not weaken assertions merely to obtain green output.

A counterexample to the handoff correction would be a successor receiving the exact recipe but still needing a materially different build because prerequisites or binding changed. A later comparable run should reuse this report, record changed inputs and assess total effort including handoff, review, helpers and recovery, rather than assuming saved tokens. No mandatory additional audit was created.

## Efficiency and accounting

`usage.json` retains own-response records aggregated from available local September 20 source logs over 09:54:28–19:54:28 UTC (ten hours), deduplicated by thread/response and excluding inherited/parent rollups. The observed coordinator tree rooted at `01a0bf28-8257-7191-8506-f66994d2fda2` has eight contributing sessions: 5,227,908 fresh input tokens, 241,570,432 cached input tokens and 536,889 output tokens (247,335,229 total). It includes observed helpers, attempts and retries through parent relationships. Cached tokens are not fresh tokens or a bill. The broader exact-workspace total is 6,977,584 fresh input, 296,118,656 cached input and 746,582 output; it also includes owner/specification/review work and is not identical to this campaign. Other local sessions are separated. Missing remote logs, sessions created on prior dates and non-token/provider billing costs are not reconstructed.

The latest sampled account-wide weekly allowance is 52% at 19:54:18 UTC with the same reset across the window. The elapsed-week pace estimate is 2.49× (used fraction divided by elapsed weekly fraction), not a supplied billing multiplier. 2,773 retained samples span 39%–52% used and estimated pace 2.49×–2.83×. This supports sustained high sampled use in this window, not continuous observation or campaign attribution of the 13 percentage-point increase. Account-wide allowance is not this campaign's consumption.

The signal predecessor is the largest observed fresh-token consumer: 1,673,782 fresh input, 60,382,976 cached and 137,878 output. Its useful native/source work is not classified as waste. The failed successor used 164,200 fresh input, 5,332,992 cached and 10,625 output; that whole attempt is an upper bound on work associated with the handoff/build detour, not measured avoidable cost. Performance claim receipts show completed useful instrumentation and native measurement work elsewhere in the tree; high volume alone is not a reason to stop that outcome.

`review-usage.json` records this review's available prefix plus completed Luna helper: reviewer 118,781 fresh input, 2,558,720 cached, 10,537 output; helper 88,032 fresh input, 1,028,864 cached, 5,313 output. Final report, closeout and later response tokens are outside that snapshot. Review/helper/reproduction cost is explicitly additional; no net savings are claimed. The observed benefit is recovering one working build-and-test route in about 52 seconds of command execution. There is no same-outcome successful successor comparison yet.

## Lifecycle and cleanup

`close_review.py` validates the exact incident on a disposable database copy before applying the diagnosis and micro resolution plus no-change macro resolution through DeadlineHarness. Completion requests exactly one fresh coordinator via the existing durable restart mechanism. The external supervisor alone may launch/acknowledge it; this reviewer does neither. Accepted records are compared by content before/after, and the original missed deadline and terminal finding remain history. `closeout.json` contains the durable result.

The reviewer-owned make/test processes completed. The failed-test user directory and temporary displaced generated outputs were removed after preserving failure logs and hashes. The correctly rebuilt test executable and useful object cache remain for delivery. Pre-existing unrelated processes and artifacts were not cleaned or claimed. The pending queue is rechecked at closeout. Stop condition: the build/handoff concern is explained and the smallest supported correction is validated; remaining product contradictions are concrete assigned work, not a reason to strand the review gate.
