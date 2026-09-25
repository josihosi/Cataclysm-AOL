# Harness reliability and worker-effort audit

Read-only diagnosis, 2026-09-20. No active method, ledger, product, game or supervisor state changed by this audit. Astra owns synthesis; two bounded Luna helpers inspected Terra's 002 and 004 source logs. Historical review findings are reused, not reinvestigated. Snapshot accounting is in `usage.json`.

## Conclusion

The strongest improvement is to remove the language model from routine execution control. The harness should own a resumable native operation and return a compact, typed result or a real decision. Scenario readiness and executable build identity need similarly concrete interfaces. This is a plausible route to substantially lower token use, not measured 50% savings. Universal 100% reliability is not supportable; a bounded supported-operation contract can be tested for zero known failures and honest classification of unknown outcomes.

## Observed work and original handles

- Luna signal 001 (thread `01a0bfbb-c2dd-7860-a762-96218ce29a60`) found smoke crossing an overmap-tile boundary. A centered control produced creation then unchanged deduplication. This was useful falsification, not a demonstrated production identity defect. Source log `rollout-2026-09-20T18-52-28-01a0bfbb-c2dd-7860-a762-96218ce29a60.jsonl`, lines 579, 835, 1513, 1563, 2198; retained `.de67/task-logs/R033-stable-omt-control-witness.json`.
- Terra signal 002 (thread `01a0bfe0-e862-75c2-b4f8-7644d31d6e35`) performed native/source investigation and reached a local-owner sound-retention contradiction. Its supported headless build route was not included in the successor's actionable handoff. Original build at its source log line 3435 and focused invocation at 3614. Prior `.de67/task-logs/review-signal-proof-003/report.md` retains the detailed diagnosis and counterevidence.
- Earlier in 002, the staged C-4 was outside the loaded map. A whole watch/countdown route therefore lacked an executable sound source. Source log line 788 repairs physical footing by moving the passive avatar and changing the source offset from `[24,72,0]` to `[0,24,0]`, with a loaded-map preflight. The retry then emitted the source but still exposed a separate downstream retention problem. Source opportunity and adapter retention were distinct failures; fixing one did not prove the other. The received packet used a whole-claim fallback rather than a narrower current frontier. The helper counted 467 custom calls and 100 play_cli calls across 002; these are workload counts, not all avoidable waste.
- Luna signal 003 (thread `01a0c057-825b-7773-8a2a-a939712a57ae`) tried direct sub-Makefile commands and manually linked game objects into a supposed test executable. Log lines 268–306, 319–374, 381, 430. Existing instructions did not require this. Restoring the predecessor's actual root-Makefile route took approximately 52 seconds of command execution and exposed two genuine remaining failures. Compilation alone had not proved test execution.
- Terra signal 004 (thread `01a0c06a-ecd4-7f73-a01a-e65302af798e`; model confirmed by turn_context) repaired the fixture/recorder and passed six cases, 606 assertions: `.de67/task-logs/R-CAOL-SIGNAL-PROOF-exploration-004/focused-test-2.log:16`. A source-bound tiles build hit incompatible PCH flags and required manual recovery (`tiles-source-build.log:1`; source log lines 501, 514, 523, 531).
- 004 automated wait used 42 native actions for 13 game minutes before `keep_watch_recipe_action_not_advertised`; only `activity.pause` was advertised. The next attempt made zero progress. Exact artifacts: `native-wait-to-dispatch-response.json`, `native-wait-to-next-dispatch-response.json`, `native-activity-pause-response.json` in that task directory. Manual recovery then used 30-minute, five-minute and one-minute waits. There is no 20-second native duration cap.
- At minute 9300, the first relevant scheduler slice applied no dispatch and recorded drive 358 below threshold 500. Exact sources: `native-scheduler-threshold-source.txt`, `native-dispatch-at-9301.json`, `native-owner-at-9301.json`. A retained lead still existed. This is first-slice non-dispatch, not proof that dispatch could never occur. No complete sound/scout-return journey was proved.

Source logs above are under `/Users/josefhorvath/.codex/sessions/2026/09/20/`. Evidence artifacts are original retrieval handles, not substitute owner instructions.

## Architecture recommendation

1. Consolidate the existing driver around a correlated operation lifecycle: accepted, running, awaiting a decision, completed, cancelled, failed, or outcome unknown. The availability of Pause must not mean the operation needs input. Track operation/session/source identity and checkpoints; reconnect to an accepted operation rather than resubmitting it. Preserve explicit danger/user decisions. Use native advertised durations and bounded game-time/milestone goals.
2. Make scenario premises executable through the production evaluator. Check source opportunity (loaded-map position, native countdown and observer eligibility) as well as initial drive, threshold and expected causal boundary, not merely fixture metadata such as member and lead counts. Reject or explain an unsuitable setup before expensive native execution. Do not lower production thresholds or inject proof-bearing state to make a scenario pass. Native execution still supplies the acceptance proof.
3. Deliver executable build/test manifests: supported command, configuration fingerprint, binary/source binding, test selector and known result. Automatically invalidate incompatible generated PCH state. Consolidate existing routes rather than creating another competing framework.
4. Return compact stable status fields for game time, operation/input ownership, milestone and failure category, with exact evidence handles for expansion. Keep the agent out of normal menu/observe/poll loops and ad hoc JSON-shape discovery.
5. Preserve useful workers across closely related tasks with a new immutable assignment and explicit proof frontier. Reuse verified artifacts even when a fresh worker is justified. Do not retain an enormous transcript merely to claim reuse.

Existing overlap to simplify: `tools/openclaw_harness/cockpit.py:1153` (`keep_watch`), `startup_harness.py:5448` (native wait successor), and `src/do_turn.cpp:10035` (`activity_wait`). Extend and consolidate these interfaces; do not wrap their ambiguity with more prose rules.

Validate using retained activity-wait transitions and fault cases (delayed frames, reconnect, stale observation, duplicate collection, cancellation, crash/unknown outcome), then a source-bound native long wait and another long activity. Separately rerun the intended gameplay journey. Compare full-tree cost for the same accepted outcome, including helper/review/retry/recovery costs. Fewer calls or passing unit tests alone do not establish savings or gameplay success.

## Sol reuse and review judgment

The first seven assigned primary tasks used seven distinct workers, but Sol repeatedly continued the same workers for corrections. The old coordinator log (`rollout-2026-09-20T16-11-38-01a0bf28-8257-7191-8506-f66994d2fda2.jsonl`) records these followups at lines 340, 466, 752, 797, 974, 1041, 1142, 1201, 1624, 1681 and 1963. Line 454 is a mistyped-target attempt, not evidence of a successful continuation.

The newer Sol log (`rollout-2026-09-20T22-02-27-01a0c069-b1b6-7740-b0e5-934599578ee3.jsonl`) explicitly says at line 600 that it is reusing 004's accumulated knowledge for the next immutable task; line 601 follows up. Line 625 records interrupted rebinding. Thus cross-task reuse is being attempted, with lifecycle friction. Line 636 acknowledges the owner's architecture proposal; lines 739/755 assign a fresh Terra worker to the shared harness lifecycle repair. Earlier pending-mailbox status is now stale.

Several review demands were substantive: malformed performance traces, false stalled-progress alarms while a menu owned input, and wall-clock comparisons contaminated by operator gaps. Sol accepted focused ceilings without pretending the whole claim passed. These support keeping the proof bar. The better optimization is reusable acceptance evidence and executable handoffs, not indiscriminately removing review or blaming model choice. This audit does not establish that every followup was necessary.

## Cost and uncertainty

Ten-hour local snapshot, 11:06–21:06 UTC: coordinator trees including observed workers/reviewer/retries used 6,013,624 fresh input, 279,595,648 cached input and 623,588 output tokens. Cached input is not fresh input and these counts are not a bill. Missing earlier-created or remote sessions and external status-agent attribution are disclosed in `usage.json`.

Account-wide weekly allowance samples rose from 40% to 55% during this window; this is not the campaign's share. Latest elapsed-week pace estimate is approximately 2.55x, calculated from allowance fraction and reset time, not a supplied billing multiplier. The prior retained ten-hour review also showed sustained high sampled use. No continuous activity or campaign attribution follows from these samples.

This audit itself had already consumed 429,229 fresh input, 6,712,448 cached input and 27,163 output tokens including both helpers at the snapshot; later completion is excluded. The investigation cost is additional. No measured reduction is claimed. Reuse this diagnosis and retained reproductions rather than repeating the log investigation without changed inputs.

Stop condition: the main failure mechanisms are explained and a supported architecture direction is identified. Implementation and same-outcome cost comparison remain owned by the active delivery work.
