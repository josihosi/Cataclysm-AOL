# R-HARNESS-EXECUTION exploration result

## Finding

The shared wait-operation lifecycle is repaired and covered by focused tests, but the assigned live **operation-interruption** proof remains unproved.  The only authorized fresh candidate with a source event (R033) produced a real C-4 consequence yet did not expose a new native input owner: both public waits completed normally.  R013 remains prelaunch-blocked by its missing retained save.  No other live-decision route is authorized by the task evidence.

## Implementation

`CockpitRunChannel` now retains a receipt-bound `caol-native-operation-v1` separately from the current surface/recipe cursor.  It projects requested duration, target, completed game-minute progress, input owner/actions, interruption policy, reconnect outcome-unknown semantics, receipt identity, and evidence handles.  Both shared recipe driving and `game.raw_wait` collect that same operation through World → wait menu → duration → activity/World without replaying an accepted wait or treating an optional `activity.pause` as mandatory.

The duration chooser selects the longest freshly advertised duration within the remaining bound.  A genuine unfamiliar native owner stops the raw operation as `native_wait_interrupted`; safe activity recovery remains receipt-bound.  The menu-parent cursor is reset after semantic duration selection, avoiding a stale duration resubmission.  These changes migrate the existing `game.keep_watch` consumer as well as the raw-wait route; no parallel operation owner was introduced.

The owner correction is applied: menu selection is zero-time while it has no scalar duration, activity descriptors `activity_wait` and `wait_activity` are running state rather than recipe intent, and a `menu.choose` transient-World successor remains in the same wait transaction.  The resulting regression was caught by `test_wait_menu_choice_reobserves_past_a_transient_world_successor` and repaired before final verification.

## Verification

`python3 -m py_compile tools/openclaw_harness/cockpit.py tools/openclaw_harness/play_cli.py tools/openclaw_harness/startup_harness.py` passed.

The 139-test focused core suite passed in 11.858s; its log is `verification-core.log`.  Coverage includes captured semantic menu transitions, receipt-bound activity collection without replay, duration-cursor reset, delayed completion/no-progress, repeated observations, safe/unsafe interruption policy, cancellation and reconnect ownership semantics, longest fresh duration selection, raw and keep-watch reuse, and the R033 delayed-source fixture guard.  `git diff --check -- tools/openclaw_harness/cockpit.py tools/openclaw_harness/cockpit_raw_wait_test.py` passed.

The separate `cockpit_file_bridge_test.py` run passed its first test, then stalled in `test_bound_startup_hud_progress_transitions_to_ready_before_requests`; the owned runner and no remaining child were terminated.  This repeated pre-existing teardown/stall is not counted as a pass.

## Public live evidence and cleanup

Normal source-bound operation:

- Run `6aa14157255c3485097af3b7a4be3cecf23624d9d1f6829e98364fc13f90bc91`, binding `dfe55542646dff0841f538c83e54ed650a8e7c3929c4236547ac50efa693741e`.
- Public `game.raw_wait` request `play-0026c50e6c1048d2b27ed53466a35b84` accepted and completed 30 game minutes, `9241 → 9271`, stop reason `target_reached`; response SHA-256 `72dd153dff599a06a56a4d5256ba9838e8ce3b108919ac552f0a877a8186aa0b`.
- Its game PID 85428, birth `Sun Sep 20 23:41:52 2026`, was later cleanly reconciled by explicit run quit/collection; scenario terminalization reported `terminated`, `native_exit_credit=false`.

Source-bound boundary and post-boundary observation:

- Run `77368cf986a9fe267375bdc4cab592d9bc2d35f217b725781d29b2b83f48e213`, binding `901fd03ebe80aaef152604370e64de1d5cf51ee0d48c597fe90ff70cf7153f1e`.
- Public request `play-cbe5b4cc8fa3457897acb649f4bc6387` completed `840` minutes, `9241 → 10081`, `target_reached`, no handled interruptions.  Response SHA-256 `fe8ec6a7c38051b83b8c43ecc7c69c326991f811c24a78d6ef140215162806d0`.
- The smallest post-boundary public wait, request `play-be02871fbb5b4859b4527e75694e3b53`, completed `30` minutes, `10081 → 10111`, again `target_reached`, `handled_interruptions=[]`.  Its native terminal facts retain `Your ears ring!` and the `deaf` effect (started 29 minutes earlier), establishing the C-4 source consequence while proving it did **not** demand an operation decision.  Response SHA-256 `a6d20b24b82accd939e4c57931091d1fd34094784e16be3a0c5f219afc258541`.
- Explicit public `run.quit` then `collect` reached `safe_to_cleanup`.  PID 86263, birth `Sun Sep 20 23:46:00 2026`, was verified absent; scenario cleanup reports `SIGTERM`, `status=terminated`, and `native_exit_credit=false`.  Bridge PID 86210 and its registry child were absent afterward.

These runs are harness-only/non-product lifecycle evidence.  They do not accept R033 signal/camp behavior, nor do they demonstrate an actual native operation interruption.

## First open boundary

Provide an authorized, source-bound live scenario that actually presents a post-acceptance native decision owner (or restore the missing R013 save and its prelaunch route).  Re-run one public wait against that scenario and retain the terminal `native_wait_interrupted` observation, partial progress, request/binding/receipt identity, then reconcile PID and broker exit.  Do not infer interruption from a source effect that leaves the World owner intact.
