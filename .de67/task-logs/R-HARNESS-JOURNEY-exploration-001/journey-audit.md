# R-HARNESS-JOURNEY exploration audit

Date: 2026-09-21. Branch `dev`, commit `fe8c418c1636bdfa6e16d0009af83873a079e24f`.
This is a partial, honest journey result; it does not close the red H-JOURNEY claim.

## 1. Largest verified waste causes

- Repeated full `look`/polling is not required for ordinary native actions; the client can reuse a returned frame. The retained long-wait/cancellation run still incurred setup, collection and closeout overhead.
- A fresh-agent unfamiliar-action path can be compact: World → Inventory → item details → close → return. The native receipts were sufficient; screenshots or GUI automation were unnecessary.
- The interruption boundary is asymmetric: a pending long wait can be cancelled and collected, but the post-cancel `activity.pause` path timed out waiting for a native receipt. Replaying would add cost without changing evidence.
- Fixture/selection failures happen before gameplay and must remain separate from gameplay proof. The ergonomics selection failed because its declared player-mutation transform target was absent.

## 2. Branch/commit and dirty inputs

The pre-repair journey used the current dirty worktree at commit `fe8c418c1636bdfa6e16d0009af83873a079e24f`; its source-bound build receipt reports product source SHA-256 `675133adfd4dedab5c526bcae2ef935f1b41b37c573d3875669d0dfc844b793b` and executable SHA-256 `d6da9425abdedd7aeb7b10e207c71fcfab8e7592380f9d37c04d84989b756ab0`. Existing dirty changes were preserved. The later source repair and replacement build are recorded in the repair addendum below. The full dirty inventory is the command output captured at closeout; relevant harness inputs are `tools/openclaw_harness/play_cli.py`, `cockpit_file_bridge.py`, `startup_harness.py`, the persistence scenario/charter, and the source-bound executable.

## 3. Source/symbol findings and impact

- `tools/openclaw_harness/startup_harness.py:5660,6513,6566,7074` contains the cancellation and native-surface receipt timeout paths observed in the live run. Observed impact: cancellation returned `player_cancelled`; the later advertised pause action returned `native_surface_receipt_timeout` without a native receipt. Inference: the boundary needs a continuation-safe receipt or a documented stop state; this run does not identify whether the defect is game, bridge, or terminalization.
- `tools/openclaw_harness/startup_harness.py:31121-31220` and `411xx-417xx` implement declared post-relaunch contracts and reporting. The persistence route accepted save-and-quit and later exposed a replacement saved-world snapshot, but the worker did not complete the continuation segment; no gameplay persistence is inferred.
- `tools/openclaw_harness/play_cli.py` preserved exact request/result handles and prevented replay after cancellation or finish. This is an observed usability strength.
- `tools/openclaw_harness/scenarios/harness.living_persistence_freeplay_mcw.json` explicitly declares `post_relaunch`; its charter correctly requires a different PID and restored-world comparison. The fresh run stopped before that gate.

## 4. Classified wait trace

The original owner-reported “10 waits / almost 50 commands” trace was not recoverable from the retained archive, so it is not claimed as reconstructed. The retained 42-action/13-minute trace is a different run and remains separate. The fresh native trace is labelled a reproduction:

| segment | classification | evidence | result |
|---|---|---|---|
| World → Inventory → details → close | gameplay-adjacent native menu choices | `play-054e6e...`, `play-25179...`, `play-8e16...`, `play-349a...` | completed |
| wait 5m | gameplay choice / scheduler boundary | `play-861840...`, game minutes 8219→8224 | completed |
| wait 30m | scheduler-bound wait and late collection | `play-2ccdab...` | completed; cancel was too late, so not interruption proof |
| wait 6h | interruption experiment | `play-e7f022...`, cancel `cancel-7cef2...`, collect response `5e3561...` | cancelled with unknown action outcome; game remained active |
| activity.pause after cancellation | supported continuation attempt | `play-c42f0...`, response `39e95c...` | native-surface receipt timeout; no replay |

## 5. Native attempts, difficult cases and blockers

- Fresh-agent unfamiliar action: persistence-freeplay was launched source-bound and the native World owner exposed `world.inventory`; the completed cannibal run then proved nested inventory details with stable item UID `3033` (lighter). The fresh ergonomics fixture was attempted and blocked before gameplay: transform target `.userdata/harness-ergonomics/save/McWilliams/#Wm9yYWlkYSBWaWNr.sav.zzip` was absent.
- Easy/representative route: cannibal freeplay completed the unfamiliar menu journey, a 5-minute native wait, and normal closeout.
- Difficult asynchronous route: 6-hour wait cancellation was real and collected, but the next native decision was not receipted. This is a truthful harness/infrastructure boundary, not product gameplay proof.
- Persistence route: `harness.living_persistence_freeplay_mcw`, run `d66fa399a37ddbdf7618c0bea2e4a3b15e4d18c3113643b3bf243f45893cb989`, binding `69e5a7c43a1b52540a5b5a93cc9c68cf5421595aeffe17e6293e240bd94cd355`; native save-and-quit and YES reached Main menu, but re-entry was not admitted. The owned game and replacement process were verified exited; the bridge was gracefully stopped after it remained terminalizing.

## 6. Scenario-family coverage

| family | inspected | attempted | completed | blocked | untested |
|---|---:|---:|---:|---:|---:|
| waiting/time-dependent | yes | yes | 5m and 30m waits | 6h continuation receipt | original bandit long-wait exact trace |
| menus/input-owner changes | yes | yes | World/Inventory/item details/prompt | none in cannibal route | ergonomics fresh-agent route |
| interruptions | yes | yes | cancellation receipt | same-operation next decision | continuation after `activity.pause` |
| launch/recovery/closeout | yes | yes | source-bound launch and graceful close | bridge terminalization handoff | reentry after saved world |
| save/reload continuity | yes | yes | native save-and-quit boundary only | post-relaunch observation | exact saved-world gameplay continuity |
| movement/navigation | registry inspected | no fresh native movement in this run | — | — | yes |
| NPC interaction/async speech | registry inspected | no | — | — | yes |
| multi-feature interaction | registry inspected | no | — | — | yes |
| evidence/witness closeout | yes | yes | journal, witness, finish | no gameplay credit | — |

Registry difficult family `R033`/drive-500 was inspected through existing registry evidence but not replayed; its gameplay claim remains outside this worker's completion.

## 7. Confusing/bloated replies and decision-complete alternatives

Retained confusing outcomes: the 30-minute request looked cancellable but was already complete when cancellation arrived; the 6-hour cancellation returned `action_outcome: unknown` while time advanced to 8397; and the advertised `activity.pause` action was rejected by `native_surface_receipt_timeout`. Decision-complete alternatives are: collect the original pending request, cancel once then collect it, look once after cancellation, and stop with the exact timeout rather than resubmit. The persistence finish response also exposed `native_save_completion: matched` but `declared_reentry_ready: false`; that combination must be reported as save-boundary evidence, not persistence success.

## 8. Prioritized minimal repair list and acceptance evidence

1. Repair the post-cancel owner/receipt boundary so a supported next decision yields either a bound native receipt or an explicit terminal state. Acceptance: repeat a changed-input cancellation run, collect the original request, perform the advertised continuation once, and retain a native receipt or a bounded failure with no orphan owner.
2. Repair or rebind the ergonomics fixture so the declared transform target exists. Acceptance: fresh selection launches to a semantic-ready owner without session-file edits.
3. Complete the persistence post-relaunch segment. Acceptance: native serializer completion, different game PID, restored World facts and one useful continued action, with no fixture reinstall.
4. Recover the original ten-waits/almost-fifty-commands archive. Acceptance: exact run/scenario/dependent-step identity, or a clearly labelled non-reconstruction.

Focused regression evidence: `focused-tests.log` records 68 tests passing across play CLI, cancellation, process exit and raw wait modules.

## 9. Measurement limits and status of old findings

Actual provider token usage was unavailable for these native runs. Command counts, response bytes and transcript-derived round trips are proxies only; no 50% H-COST savings claim is made. Fresh/cached input, output tokens, helper/reviewer/retry costs and elapsed model time were not recorded by the native bridge. The build identity and exact artifacts are available, but account-wide allowance is not campaign consumption. The original long-wait failure and ten-waits trace remain unverified; the 42-action/13-minute run is not conflated with them. The normal wait/performance evidence is retained at its original scope. The fresh persistence result is useful launch/save-boundary evidence but not a completed gameplay or persistence claim.

## 10. Repair addendum

The post-cancel owner diagnosis was confirmed in `src/do_turn.cpp:10121-10130`: `activity.pause` was being accepted with a deferred receipt while the cancellation prompt owned the next native input, and the prompt could exit without republishing the activity surface. The repair keeps the native pause dispatch but returns an immediate accepted receipt. A current source-bound tiles build was produced with product source SHA-256 `a76a113102a5ec7e9b23a420d5cfecb7bb2f1d1d5c70cf18a3ac0ee034933b58` and executable SHA-256 `e86ec67a5435570e6152060f59b37b19e55e783eb0eb54a7341e922ffae2a5bb`.

Focused verification passed 10 tests (`repair-focused-tests-3.log`). A changed-input native rerun used run `bf8382ce6ec1e1713a8e5e6c88e6d8a79e3c9248f5dcb3a6edf2878597bf4970`, binding `316a3303ed7062db97a84e66d56fb2908789d48b5a10f42cc9f82c6360927c2a`, game PID `31777`, and wait request `play-1896caa20a92442aacc112b1ef7a2563`; cooperative cancel `cancel-9b747f985c1d9b49929a21491f0c740404c6ab387b0e081a45f54d5578f60d54` produced a bound `player_cancelled` receipt and progress 8219→8579. The native trace emitted `activity_wait` frames advertising `activity.pause`, but collection crossed the short activity window and no accepted pause receipt was observed. Additional source-bound reruns are summarized in `repair-rerun-summary.json`.

Disposition: the source repair and focused harness contracts are verified; live native `activity.pause` receipt acceptance remains unproven and is not claimed as complete. The original `play-c42f0da70c9d42dc942aecbc7ea36b4a` timeout remains preserved as negative evidence and was not replayed.

## 11. Dispatch-path test and changed-input native rerun

The smallest production-path regression was added at `tests/semantic_surface_test.cpp:155-197`: it models the `activity_wait` owner, submits the advertised `activity.pause`, asserts an immediate accepted receipt with no successor frame, then opens the separate cancellation prompt and verifies no receipt is stranded or duplicated. `tools/openclaw_harness/activity_resume_owner_test.py:26-29` also asserts that the production callback returns the immediate-receipt form. The exact C++ case passed 11 assertions; the Python owner/cancellation contract suite passed 5 tests. The broad semantic tag run remains non-green only because two unrelated existing tests failed (`committed dialogue effects...` and `string prompt rejects malformed text payloads...`); they are preserved as separate findings.

After the test change, the source-bound builder reran successfully with captured head `fe8c418c16`, product source SHA-256 `a76a113102a5ec7e9b23a420d5cfecb7bb2f1d1d5c70cf18a3ac0ee034933b58`, runtime source SHA-256 `8f77ba026cfc83533a76a188de1df6792a6f68d9a3ca965401cc1386f1b14a7f`, executable SHA-256 `e86ec67a5435570e6152060f59b37b19e55e783eb0eb54a7341e922ffae2a5bb`, and build log `repair-dispatch-test-build.log`.

A fresh repair-authorized native session used run `7b64d473dbb3b7354a1f6638974105466dcb093129bbf9455b84fc5ebbf7d6c5`, binding `57a60251ef6c1a6d1eed62b9e7b176c2a421c740df2aab3fde6fe9fab28cde0e`, game PID `36926`, and trace `.userdata/harness-cannibal-freeplay/harness_runs/20260921_141851_6cfc83d74f3c4591b4f8294dd7308f2b/semantic.native.events.jsonl`. The repaired binary emitted 469 native `activity_wait` descriptors (frames 7–481; game minutes 8220–9300), all advertising only `activity.pause`, and three accepted `wait.6h` receipts. A fresh `activity.pause` was submitted only through the live client, but every attempted pause request was rejected as `stale_observation` because the wait loop republished the owner faster than the client could refresh; no `native_surface_receipt_timeout` occurred. The run was closed with `play-20c11725af8240e8a695c99242783072`, response SHA-256 `d7b340940007b85d9cedcace1c4405750318f73cc7d44e0f2c6a7f75b120f1b8`, and terminalization reached `safe_to_cleanup` with child exit code 0; no gameplay credit is assigned.

This changed-input run therefore strengthens the repair boundary but does not prove live accepted `activity.pause`. The remaining live acceptance gap is an observation-window/receipt timing issue, not evidence that the repaired immediate receipt still times out. No further replay is necessary for this audit.
